/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */


#include "ngx_dynamic_healthcheck_peer.h"


static ngx_inline ngx_msec_t
current_msec()
{
    ngx_time_t *tp = ngx_timeofday();
    return tp->sec * 1000 + tp->msec;
}


static ngx_int_t
test_connect(ngx_connection_t *c)
{
    int        err;
    socklen_t  len;

#if (NGX_HAVE_KQUEUE)

    if (ngx_event_flags & NGX_USE_KQUEUE_EVENT)  {
        err = c->write->kq_errno ? c->write->kq_errno : c->read->kq_errno;

        if (err) {
            (void) ngx_connection_error(c, err,
                           (char *) "kevent() reported that connect() failed");
            return NGX_ERROR;
        }

    } else
#endif
    {
        err = 0;
        len = sizeof(int);

        /*
         * BSDs and Linux return 0 and set a pending error in err
         * Solaris returns -1 and sets errno
         */

        if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len)
            == -1)
            err = ngx_socket_errno;
        else
            ngx_socket_errno = err;

        if (err)
            return NGX_ERROR;
    }

    return NGX_OK;
}


static ngx_int_t
handle_event(ngx_event_t *ev)
{
    ngx_connection_t  *c = (ngx_connection_t *) ev->data;

    if (ev->write) {
        if (ngx_handle_write_event(c->write, 0) == NGX_OK)
            return NGX_OK;

        test_connect(c);
        return NGX_ERROR;
    }

    if (ngx_handle_read_event(c->read, 0) == NGX_OK)
        return NGX_OK;

    test_connect(c);
    return NGX_ERROR;
}


ngx_int_t
ngx_dynamic_healthcheck_peer::handle_io(ngx_event_t *ev)
{
    ngx_connection_t              *c;
    ngx_dynamic_healthcheck_peer  *peer;

    if (ev->ready) {

        if (handle_event(ev) == NGX_OK)
            return NGX_OK;

        c = (ngx_connection_t *) ev->data;
        peer = (ngx_dynamic_healthcheck_peer *) c->data;

        ngx_log_error(NGX_LOG_ERR, c->log, ngx_socket_errno,
                      "[%V] %V: %V addr=%V, fd=%d handle io",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);

        return NGX_ERROR;
    }

    return NGX_OK;
}


void
ngx_dynamic_healthcheck_peer::abort()
{
    close();
    completed();
}


void
ngx_dynamic_healthcheck_peer::fail(ngx_flag_t skip)
{
    close();

    state.shared->fall_total++;
    if (++state.shared->fall >= opts->fall) {
        state.shared->rise = 0;
        down(skip);
        state.shared->down = 1;
    }

    completed();
}


void
ngx_dynamic_healthcheck_peer::success()
{
    if (state.local->pc.connection->error)
        return fail();

    if (state.local->pc.connection)
        state.local->pc.connection->requests++;

    set_keepalive();

    state.shared->rise_total++;
    if (++state.shared->rise >= opts->rise || state.shared->fall_total == 0) {
        state.shared->fall = 0;
        up();
        state.shared->down = 0;
    }

    completed();
}


void
ngx_dynamic_healthcheck_peer::handle_idle(ngx_event_t *ev)
{
    ngx_connection_t             *c = (ngx_connection_t *) ev->data;
    ngx_dynamic_hc_local_node_t  *state =
        (ngx_dynamic_hc_local_node_t *) c->data;

    c->log->action = (char *) "idle";

    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "[%V] %V: %V addr=%V, fd=%d handle_idle()",
                   &state->module, &state->upstream,
                   &state->server, &state->name, c->fd);

    if (handle_event(ev) == NGX_ERROR)
        goto close;

    if (state->expired <= current_msec())
        goto close;

    if (ngx_stopping())
        goto close;

    ngx_add_timer(c->write, 1000);

    return;

close:

    ngx_close_connection(c);
    ngx_memzero(&state->pc, sizeof(ngx_peer_connection_t));
}


void
ngx_dynamic_healthcheck_peer::handle_connect(ngx_event_t *ev)
{
    ngx_connection_t             *c = (ngx_connection_t *) ev->data;
    ngx_dynamic_healthcheck_peer *peer =
        (ngx_dynamic_healthcheck_peer *) c->data;

    c->log->action = (char *) "connecting";

    if (ngx_stopping())
        return peer->abort();

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ERR, c->log, NGX_ETIMEDOUT,
                      "[%V] %V: %V addr=%V, fd=%d connect timed out",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);

        return peer->fail();
    }

    if (test_connect(c) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, c->log, ngx_socket_errno,
                      "[%V] %V: %V addr=%V, fd=%d connect error",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);

        return peer->fail();
    }

    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "[%V] %V: %V addr=%V, fd=%d handle_connect()",
                   &peer->module, &peer->upstream,
                   &peer->server, &peer->name, c->fd);

    if (peer->handle_io(ev) == NGX_ERROR)
        return peer->fail();

    peer->check_state = st_connected;

    c->read->handler = &ngx_dynamic_healthcheck_peer::handle_dummy;
    c->write->handler = &ngx_dynamic_healthcheck_peer::handle_write;

    ngx_add_timer(c->write, peer->opts->timeout);
    ngx_dynamic_healthcheck_peer::handle_write(c->write);
}


void
ngx_dynamic_healthcheck_peer::handle_write(ngx_event_t *ev)
{
    ngx_connection_t             *c = (ngx_connection_t *) ev->data;
    ngx_dynamic_healthcheck_peer *peer =
        (ngx_dynamic_healthcheck_peer *) c->data;
    ngx_int_t                     rc;

    c->log->action = (char *) "sending request";

    if (ngx_stopping()) {
        ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "[%V] %V: %V addr=%V, fd=%d worker stopping, close",
                       &peer->module, &peer->upstream,
                       &peer->server, &peer->name, c->fd);
        return peer->abort();
    }

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ERR, c->log, NGX_ETIMEDOUT,
                      "[%V] %V: %V addr=%V, fd=%d write request timed out",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);

        return peer->fail();
    }

    if (peer->check_state != st_connected && peer->check_state != st_sending) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0,
                      "[%V] %V: %V addr=%V, fd=%d invalid state",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);
        return peer->fail();
    }

    peer->check_state = st_sending;

    ngx_shmtx_lock(&peer->state.shared->state->slab->mutex);

    rc = peer->on_send(peer->state.local);

    ngx_shmtx_unlock(&peer->state.shared->state->slab->mutex);

    ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "[%V] %V: %V addr=%V, fd=%d on_send(), rc=%d",
                   &peer->module, &peer->upstream,
                   &peer->server, &peer->name, c->fd, rc);

    if (peer->handle_io(ev) == NGX_ERROR)
        return peer->fail();

    switch(rc) {
        case NGX_DECLINED:
            ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d handle_write() declined",
                           &peer->module, &peer->upstream,
                           &peer->server, &peer->name, c->fd);

        case NGX_OK:
            break;

        case NGX_AGAIN:
            return;

        case NGX_ERROR:
        default:
            return peer->fail();
    }

    ngx_del_timer(c->write);

    peer->check_state = st_sent;

    c->read->handler = &ngx_dynamic_healthcheck_peer::handle_read;
    c->write->handler = &ngx_dynamic_healthcheck_peer::handle_dummy;

    peer->state.local->buf->pos = peer->state.local->buf->start;
    peer->state.local->buf->last = peer->state.local->buf->start;

    ngx_add_timer(c->read, peer->opts->timeout);
    ngx_dynamic_healthcheck_peer::handle_read(c->read);
}


void
ngx_dynamic_healthcheck_peer::handle_read(ngx_event_t *ev)
{
    ngx_connection_t             *c = (ngx_connection_t *) ev->data;
    ngx_dynamic_healthcheck_peer *peer =
        (ngx_dynamic_healthcheck_peer *) c->data;
    ngx_int_t                     rc;

    c->log->action = (char *) "receiving response";
    
    if (ngx_stopping()) {
        ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "[%V] %V: %V addr=%V, fd=%d worker stopping, close",
                       &peer->module, &peer->upstream,
                       &peer->server, &peer->name, c->fd);
        return peer->abort();
    }

    if (ev->timedout) {
        ngx_log_error(NGX_LOG_ERR, c->log, NGX_ETIMEDOUT,
                      "[%V] %V: %V addr=%V, fd=%d read response timed out",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);
        return peer->fail();
    }

    if (peer->check_state != st_sent && peer->check_state != st_receiving) {
        ngx_log_error(NGX_LOG_ERR, c->log, 0,
                      "[%V] %V: %V addr=%V, fd=%d invalid state",
                      &peer->module, &peer->upstream,
                      &peer->server, &peer->name, c->fd);

        return peer->fail();
    }

    peer->check_state = st_receiving;

    ngx_shmtx_lock(&peer->state.shared->state->slab->mutex);

    rc = peer->on_recv(peer->state.local);

    ngx_shmtx_unlock(&peer->state.shared->state->slab->mutex);

    ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "[%V] %V: %V addr=%V, fd=%d on_recv(), rc=%d",
                   &peer->module, &peer->upstream,
                   &peer->server, &peer->name, c->fd, rc);

    if (peer->handle_io(ev) == NGX_ERROR)
        return peer->fail();

    switch(rc) {
        case NGX_DECLINED:
            ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d handle_read() declined",
                           &peer->module, &peer->upstream,
                           &peer->server, &peer->name, c->fd);

        case NGX_OK:
            break;

        case NGX_AGAIN:
            return;

        case NGX_ERROR:
        default:
            return peer->fail();
    }

    ngx_del_timer(c->read);

    peer->success();
}


void
ngx_dynamic_healthcheck_peer::handle_dummy(ngx_event_t *ev)
{
    ngx_connection_t             *c = (ngx_connection_t *) ev->data;
    ngx_dynamic_healthcheck_peer *peer =
        (ngx_dynamic_healthcheck_peer *) c->data;

    if (ngx_stopping())
        return peer->abort();

    test_connect(c);

    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, ngx_socket_errno,
                   "[%V] %V: %V addr=%V, fd=%d handle_dummy()",
                   &peer->module, &peer->upstream,
                   &peer->server, &peer->name, c->fd);

    if (!ev->ready)
        return;

    if (peer->handle_io(ev) == NGX_ERROR)
        return peer->fail();
}


ngx_int_t
ngx_dynamic_healthcheck_peer::peek()
{
    char               buf[1];
    ngx_connection_t  *c = state.local->pc.connection;
    ngx_int_t          rc = recv(c->fd, buf, 1, MSG_PEEK);

    ngx_log_debug6(NGX_LOG_DEBUG_HTTP, state.local->pc.connection->log,
                   ngx_socket_errno, "[%V] %V: %V addr=%V, fd=%d peek(), rc=%d",
                   &module, &upstream, &server, &name, c->fd, rc);

    if (rc == 1)
        return NGX_OK;

    if (rc == -1 && ngx_socket_errno == NGX_EAGAIN) {
        c->read->ready = 0;
        if (ngx_handle_read_event(c->read, 0) == NGX_OK)
            return NGX_OK;
    }

    return NGX_ERROR;
}


void
ngx_dynamic_healthcheck_peer::close()
{
    ngx_connection_t  *c = state.local->pc.connection;

    if (c != NULL) {
        ngx_log_debug5(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "[%V] %V: %V addr=%V, fd=%d close()",
                       &module, &upstream, &server, &name, c->fd);
        ngx_close_connection(c);
    }

    ngx_memzero(&state.local->pc, sizeof(ngx_peer_connection_t));
}


void
ngx_dynamic_healthcheck_peer::set_keepalive()
{
    ngx_connection_t  *c = state.local->pc.connection;

    if (c == NULL)
        return;

    if (c->read->pending_eof)
        goto close;

    if (ngx_stopping())
        goto close;

    if (opts->keepalive > 1) {
        ngx_log_debug7(NGX_LOG_DEBUG_HTTP, c->log, 0,
                      "[%V] %V: %V addr=%V, fd=%d set_keepalive(),"
                      " requests=%d of %d",
                      &module, &upstream, &server, &name, c->fd,
                      c->requests, opts->keepalive);
    }

    if (c->error || c->requests >= opts->keepalive)
        goto close;

    state.local->expired = current_msec() + 4 * opts->interval * 1000;

    c->write->handler = &ngx_dynamic_healthcheck_peer::handle_idle;
    c->read->handler = &ngx_dynamic_healthcheck_peer::handle_idle;
    c->data = state.local;

    return ngx_add_timer(c->write, 1000);

close:

    close();
}


void
ngx_dynamic_healthcheck_peer::connect()
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    if (state.local->pc.connection != NULL) {
        c = state.local->pc.connection;

        if ((rc = peek()) == NGX_OK) {
            ngx_log_debug5(NGX_LOG_DEBUG_HTTP, event->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d connect(),"
                           " reuse connection",
                           &module, &upstream, &server, &name,
                           c->fd);
            goto connected;
        }

        close();
    }

    ngx_memzero(&state.local->pc, sizeof(ngx_peer_connection_t));

    state.local->pc.sockaddr = state.local->sockaddr;
    state.local->pc.socklen = state.local->socklen;
    state.local->pc.name = &state.local->name;
    state.local->pc.get = ngx_event_get_peer;
    state.local->pc.log = ngx_cycle->log;
    state.local->pc.log_error = NGX_ERROR_ERR;

    rc = ngx_event_connect_peer(&state.local->pc);

    if (rc == NGX_ERROR || rc == NGX_DECLINED || rc == NGX_BUSY)
        return fail();

    c = state.local->pc.connection;

    ngx_log_debug5(NGX_LOG_DEBUG_HTTP, event->log, 0,
                   "[%V] %V: %V addr=%V, fd=%d connect()",
                   &module, &upstream, &server, &name,
                   c->fd);

connected:

    c->pool = state.local->pool;
    c->log = ngx_cycle->log;
    c->sendfile = 0;
    c->read->log = ngx_cycle->log;
    c->write->log = ngx_cycle->log;
    c->data = this;

    if (rc != NGX_AGAIN) {
        if (test_connect(c) == NGX_ERROR) {
            ngx_log_error(NGX_LOG_ERR, c->log, ngx_socket_errno,
                        "[%V] %V: %V addr=%V, fd=%d connect error",
                        &this->module, &this->upstream,
                        &this->server, &this->name, c->fd);

            this->fail();
            return;
        }
        check_state = st_connected;
        c->write->handler = &ngx_dynamic_healthcheck_peer::handle_write;
        c->read->handler = &ngx_dynamic_healthcheck_peer::handle_dummy;
        ngx_add_timer(c->write, opts->timeout);
        ngx_dynamic_healthcheck_peer::handle_write(c->write);
        return;
    }

    /* NGX_AGAIN */

    c->write->handler = &ngx_dynamic_healthcheck_peer::handle_connect;
    c->read->handler = &ngx_dynamic_healthcheck_peer::handle_connect;

    check_state = st_connecting;

    ngx_add_timer(c->write, opts->timeout);
}


void
ngx_dynamic_healthcheck_peer::completed()
{
    check_state = st_done;

    ngx_log_error(NGX_LOG_INFO, event->log, 0,
                  "[%V] %V: %V addr=%V completed",
                  &module, &upstream, &server, &name);

    this->~ngx_dynamic_healthcheck_peer();

    ngx_free(this);
}


ngx_dynamic_healthcheck_peer::ngx_dynamic_healthcheck_peer
    (ngx_dynamic_healthcheck_event_t *ev, ngx_dynamic_hc_state_node_t s)
        : opts(ev->conf->shared), state(s), event(ev)
{
    ngx_connection_t  *c = state.local->pc.connection;

    if (c != NULL) {
        if (c->write->timer_set)
            ngx_del_timer(c->write);
        if (c->read->timer_set)
            ngx_del_timer(c->read);

        c->write->timedout = 0;
        c->read->timedout = 0;

        c->read->ready = 0;
        c->read->ready = 0;
    }

    state.local->buf->pos = state.local->buf->last = state.local->buf->start;
}


void
ngx_dynamic_healthcheck_peer::check()
{
    static const ngx_str_t skip_addr = ngx_string("0.0.0.0");

    ngx_time_t  *tp = ngx_timeofday();

    if (ngx_stopping()) {

        close();
        goto end;
    }

    if (name.len >= skip_addr.len
        && ngx_memcmp(name.data, skip_addr.data, skip_addr.len) == 0) {
        down();
        return completed();
    }

    if (opts->disabled)
        goto disabled;

    if (ngx_peer_disabled(&name, event->conf)
        || ngx_peer_disabled(&server, event->conf))
        goto disabled;

    if (ngx_peer_excluded(&name, event->conf)
        || ngx_peer_excluded(&server, event->conf))
        goto excluded;

    if (state.shared->checked + opts->interval > tp->sec)
        goto end;

    return connect();

disabled:

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, event->log, 0,
                   "[%V] %V: %V addr=%V disabled",
                   &module, &upstream, &server, &name);

    close();
    down();

    goto end;

excluded:

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, event->log, 0,
                   "[%V] %V: %V addr=%V exclude",
                   &module, &upstream, &server, &name);

end:

    this->~ngx_dynamic_healthcheck_peer();

    ngx_free(this);
}


ngx_dynamic_healthcheck_peer::~ngx_dynamic_healthcheck_peer()
{
    ngx_time_t  *tp = ngx_timeofday();
    if (state.shared->checked + opts->interval <= tp->sec)
        state.shared->checked = tp->sec;
}


ngx_int_t
ngx_dynamic_healthcheck_match_buffer(ngx_str_t *pattern, ngx_str_t *s)
{
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    if (s->data == NULL) {
        s->len = 0;
        s->data = (u_char *) "";
    }

    rc.pattern = *pattern;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;
#ifdef NGX_REGEX_DOTALL
    rc.options = NGX_REGEX_DOTALL;
#endif

    rc.pool = ngx_create_pool(1024, ngx_cycle->log);
    if (rc.pool == NULL) {
        ngx_log_error(NGX_LOG_CRIT, ngx_cycle->log, 0, "match: no memory");
        return NGX_ERROR;
    }

    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_destroy_pool(rc.pool);
        return NGX_ERROR;
    }

    int captures[(1 + rc.captures) * 3];
    int m = ngx_regex_exec(rc.regex, s, captures, (1 + rc.captures) * 3);

    ngx_destroy_pool(rc.pool);

    if (m == NGX_REGEX_NO_MATCHED)
        return NGX_DECLINED;

    return m >= 0 ? NGX_OK : NGX_ERROR;
}

/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_HTTP_H
#define NGX_DYNAMIC_HEALTHCHECK_HTTP_H


#include "ngx_dynamic_healthcheck_tcp.h"

extern "C" ngx_int_t
ngx_http_read_header(ngx_http_request_t *r, ngx_buf_t *buf, ngx_keyval_t *h);


template <class PeerT> class ngx_dynamic_healthcheck_http :
    public ngx_dynamic_healthcheck_tcp<PeerT>
{
    ngx_http_request_t  r;
    ngx_http_status_t   status;
    ngx_int_t           content_length;
    ngx_flag_t          chunked;
    ngx_buf_t           body;
    ngx_pool_t         *pool;

protected:

    ngx_int_t
    make_request(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_buf_t                       *buf = state->buf;
        ngx_connection_t                *c = state->pc.connection;
        ngx_uint_t                       i;
        ngx_dynamic_healthcheck_opts_t  *shared = this->shared;

        buf->last = ngx_snprintf(buf->last, buf->end - buf->last,
            "%V %V HTTP/1.%s\r\n",
            &shared->request_method,
            &shared->request_uri,
            shared->keepalive > c->requests + 1 ? "1" : "0");

        if (state->server.len >= 5
            && ngx_strncmp(state->server.data, "unix:", 5) != 0)
            buf->last = ngx_snprintf(buf->last, buf->end - buf->last,
                "Host: %V\r\n", &state->name.str);

        buf->last = ngx_snprintf(buf->last, buf->end - buf->last,
            "User-Agent: nginx/"NGINX_VERSION"\r\n"
            "Connection: %s\r\n",
            shared->keepalive > c->requests + 1 ? "keep-alive" : "close");

        for (i = 0; i < shared->request_headers.len; i++)
            buf->last = ngx_snprintf(buf->last, buf->end - buf->last,
                "%V: %V\r\n",
                &shared->request_headers.data[i].key,
                &shared->request_headers.data[i].value);

        if (shared->request_body.len)
            buf->last = ngx_snprintf(buf->last, buf->end - buf->last,
                "Content-Length: %d\r\n\r\n%V",
                shared->request_body.len, &shared->request_body);
        else
            buf->last = ngx_snprintf(buf->last, buf->end - buf->last, "\r\n");

        if (buf->last == buf->end) {
            ngx_log_error(NGX_LOG_WARN, c->log, 0,
                          "[%V] %V: %V addr=%V, fd=%d http "
                          "healthcheck_buffer_size too small for the request",
                          &this->module, &this->upstream,
                          &this->server, &this->name, c->fd);
            return NGX_ERROR;
        }

        return NGX_OK;
    }
    
    virtual ngx_int_t
    on_send(ngx_dynamic_hc_local_node_t *state)
    {
        if (this->event->conf->shared->request_uri.len == 0)
            goto tcp;

        if (state->buf->last == state->buf->start)
            if (make_request(state) == NGX_ERROR)
                return NGX_ERROR;

tcp:

        return ngx_dynamic_healthcheck_tcp<PeerT>::on_send(state);
    }

    virtual ngx_int_t
    on_recv(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_connection_t                *c = state->pc.connection;
        ngx_uint_t                       i;
        ngx_dynamic_healthcheck_opts_t  *shared = this->shared;

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, state->pc.connection->log, 0,
                       "[%V] %V: %V addr=%V, fd=%d http on_recv() %s",
                       &this->module, &this->upstream,
                       &this->server, &this->name, c->fd,
                       body.start == NULL ? "start" : "continue");

        if (body.start != NULL)
            goto recv_body;

        for (;;) {

            // receiving status line

            switch (receive_status_line(state)) {
                case NGX_OK:
                    break;

                case NGX_AGAIN:
                    goto recv_buf;

                case NGX_ERROR:
                default:
                    return NGX_ERROR;
            }

            // receiving headers

            switch (receive_headers(state)) {
                case NGX_HTTP_PARSE_HEADER_DONE:
                    goto recv_body;

                case NGX_AGAIN:
                    goto recv_buf;

                case NGX_ERROR:
                default:
                    return NGX_ERROR;
            }

recv_buf:

            switch (receive_buf(state)) {
                case NGX_OK:
                case NGX_DONE:
                    break;

                case NGX_AGAIN:
                    return NGX_AGAIN;

                case NGX_ERROR:
                default:
                    return NGX_ERROR;
            }
        }

recv_body:

        // receiving body

        if (body.start == NULL) {

            if (!chunked) {

                if (content_length == 0)
                    goto well_done;

                if (content_length == -1)
                    content_length = shared->buffer_size - 1;
            } else
                content_length = 0;

            if (shared->buffer_size - 1 < (size_t) content_length) {
                ngx_log_error(NGX_LOG_WARN, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d http "
                              "healthcheck_buffer_size too small for read body",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }

            pool = ngx_create_pool(1024, this->event->log);
            if (pool == NULL) {
                ngx_log_error(NGX_LOG_WARN, c->log, c->fd, 0,
                              "[%V] %V: %V addr=%V, fd=%d http receiving body: "
                              "no memory for read body",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }

            body.start = (u_char *) ngx_palloc(pool, shared->buffer_size);
            if (body.start == NULL) {
                ngx_log_error(NGX_LOG_WARN, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d http receiving body: "
                              "no memory for read body",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }

            body.pos = body.last = body.start;
            body.end = body.start + shared->buffer_size;
        }

        for (;;) {

            switch (receive_body(state)) {
                case NGX_OK:
                    goto well_done;

                case NGX_AGAIN:
                    break;
                    
                case NGX_ERROR:
                default:
                    return NGX_ERROR;
            }

            switch (receive_buf(state)) {
                case NGX_OK:
                    goto well_done;

                case NGX_DONE:
                    break;

                case NGX_AGAIN:
                    return NGX_AGAIN;
                    
                case NGX_ERROR:
                default:
                    return NGX_ERROR;
            }
        }

well_done:

        ngx_str_t s;
        s.data = body.start;
        s.len = body.last - body.start;

        if (s.len) {
            ngx_log_debug6(NGX_LOG_DEBUG_HTTP,
                           state->pc.connection->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d "
                           "http on_recv() body:\n%V",
                           &this->module, &this->upstream,
                           &this->server, &this->name, c->fd,
                           &s);
        }

        if (shared->response_codes.len) {
            for (i = 0; i < shared->response_codes.len; i++)
                if (shared->response_codes.data[i] == (ngx_int_t) status.code)
                    break;
            if (i == shared->response_codes.len) {
                ngx_log_error(NGX_LOG_WARN, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d http status "
                              "is not in 'check_response_codes'",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }
        }

        if (shared->response_body.len) {
            switch(ngx_dynamic_healthcheck_match_buffer(&shared->response_body,
                                                        &s)) {
                case NGX_OK:
                    ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                                   "[%V] %V: %V addr=%V, fd=%d http pattern"
                                   " '%V' found",
                                   &this->module, &this->upstream,
                                   &this->server, &this->name,
                                   c->fd, &shared->response_body);
                    return NGX_OK;

                case NGX_ERROR:
                    ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                                   "[%V] %V: %V addr=%V, fd=%d http pattern"
                                   "'%V' error",
                                   &this->module, &this->upstream,
                                   &this->server, &this->name,
                                   c->fd, &shared->response_body);
                    return NGX_ERROR;

                case NGX_DECLINED:
                default:
                    ngx_log_error(NGX_LOG_WARN, c->log, 0,
                                  "[%V] %V: %V addr=%V, fd=%d http pattern"
                                  " '%V' is not found",
                                  &this->module, &this->upstream,
                                  &this->server, &this->name, c->fd,
                                  &shared->response_body);
                    return NGX_ERROR;
            }
        }

        return NGX_OK;
    }

    ngx_int_t
    receive_buf(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_connection_t  *c = state->pc.connection;
        ngx_buf_t         *buf = state->buf;
        ssize_t            size;

        if (content_length > buf->end - buf->last) {
            ngx_log_error(NGX_LOG_WARN, c->log, 0,
                          "[%V] %V: %V addr=%V, fd=%d healthcheck_buffer_size "
                          "too small for read body",
                          &this->module, &this->upstream,
                          &this->server, &this->name, c->fd);
            return NGX_ERROR;
        }

        if (content_length == 0)
            size = c->recv(c, buf->last, buf->end - buf->last);
        else
            size = c->recv(c, buf->last, content_length);

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "[%V] %V: %V addr=%V, "
                       "fd=%d http on_recv() recv: %d",
                       &this->module, &this->upstream,
                       &this->server, &this->name, c->fd,
                       size);

        if (size == NGX_ERROR)
            return c->read->pending_eof ? NGX_OK : NGX_ERROR;

        if (size == NGX_AGAIN)
            return NGX_AGAIN;

        buf->last += size;

        return c->read->pending_eof ? /* closed */ NGX_OK : NGX_DONE;
    }

    ngx_int_t
    receive_status_line(ngx_dynamic_hc_local_node_t *state)
    {
        if (status.code != 0)
            return NGX_OK;

        switch (ngx_http_parse_status_line(&r, state->buf, &status)) {
            case NGX_OK:
                ngx_log_debug6(NGX_LOG_DEBUG_HTTP,
                               state->pc.connection->log, 0,
                               "[%V] %V: %V addr=%V, "
                               "fd=%d http on_recv() status: %d",
                               &this->module, &this->upstream,
                               &this->server, &this->name,
                               state->pc.connection->fd,
                               status.code);
                break;

            case NGX_AGAIN:
                return NGX_AGAIN;

            case NGX_ERROR:
            default:
                return NGX_ERROR;
        }

        return NGX_OK;
    }

    ngx_int_t
    receive_headers(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_keyval_t       h;
        ngx_int_t          rc;

        for (;;) {
            rc = ngx_http_read_header(&r, state->buf, &h);

            ngx_log_debug6(NGX_LOG_DEBUG_HTTP,
                           state->pc.connection->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d http"
                           " on_recv() ngx_http_read_header, rc=%d",
                           &this->module, &this->upstream,
                           &this->server, &this->name,
                           state->pc.connection->fd, rc);

            if (rc == NGX_OK) {
                if (ngx_strcmp(h.key.data, "content-length") == 0)
                    content_length = ngx_atoi(h.value.data, h.value.len);

                if (ngx_strcmp(h.key.data, "transfer-encoding") == 0)
                    chunked = ngx_strcmp(h.value.data, "chunked") == 0;

                ngx_log_debug7(NGX_LOG_DEBUG_HTTP,
                               state->pc.connection->log, 0,
                               "[%V] %V: %V addr=%V, "
                               "fd=%d http on_recv() header: %V=%V",
                               &this->module, &this->upstream,
                               &this->server, &this->name,
                               state->pc.connection->fd,
                               &h.key, &h.value);
                continue;
            }

            if (rc == NGX_AGAIN)
                return NGX_AGAIN;

            if (rc == NGX_HTTP_PARSE_HEADER_DONE)
                break;

            if (rc == NGX_DECLINED)
                continue;

            return NGX_ERROR;
        }

        return NGX_HTTP_PARSE_HEADER_DONE;
    }

    ngx_int_t
    receive_body(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_connection_t  *c = state->pc.connection;
        ssize_t            size;
        u_char            *sep;
        ngx_buf_t         *buf = state->buf;

        if (chunked) {

again:
            
            if (content_length != 0) {
                size = ngx_min(buf->last - buf->pos, content_length);
                ngx_memcpy(body.last, buf->pos, size);
                body.last += size;
                buf->pos += size;
                content_length -= size;
                if (content_length > 0)
                    return NGX_AGAIN;
                buf->pos += 2;  // CRLF
            }

            if (buf->pos == buf->last) {
                buf->pos = buf->last = buf->start;
                return NGX_AGAIN;
            }

            sep = (u_char *) ngx_strstr(buf->pos, CRLF);
            if (sep == NULL)
                return NGX_AGAIN;

            content_length = ngx_hextoi(buf->pos, sep - buf->pos);
            if (content_length < 0) {
                ngx_log_error(NGX_LOG_WARN, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d http "
                              "invalid chunk size",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }

            if (content_length == 0) {
                *body.last = 0;
                return NGX_OK;
            }

            ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                           "[%V] %V: %V addr=%V, fd=%d http"
                           " on_recv() body chunk, size=%d",
                           &this->module, &this->upstream,
                           &this->server, &this->name, c->fd,
                           content_length);

            if (content_length > body.end - body.last - 1) {
                ngx_log_error(NGX_LOG_WARN, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d "
                              "healthcheck_buffer_size too small for read body",
                              &this->module, &this->upstream,
                              &this->server, &this->name, c->fd);
                return NGX_ERROR;
            }

            buf->pos = sep + 2;  //CRLF after chunk size

            goto again;
        }

        if ((size_t) content_length > this->shared->buffer_size - 1) {
            ngx_log_error(NGX_LOG_WARN, c->log, 0,
                          "[%V] %V: %V addr=%V, fd=%d "
                          "healthcheck_buffer_size too small for read body",
                          &this->module, &this->upstream,
                          &this->server, &this->name, c->fd);
            return NGX_ERROR;
        }

        ngx_memcpy(body.last, buf->pos, buf->last - buf->pos);
        body.last += buf->last - buf->pos;
        content_length -= buf->last - buf->pos;
        buf->pos = buf->last = buf->start;

        if (content_length == 0) {
            *body.last = 0;
            return NGX_OK;
        }

        return NGX_AGAIN;
    }
    
public:

    ngx_dynamic_healthcheck_http(PeerT *peer,
        ngx_dynamic_healthcheck_event_t *event, ngx_dynamic_hc_state_node_t s)
        : ngx_dynamic_healthcheck_tcp<PeerT>(peer, event, s),
          content_length(-1), chunked(0), pool(NULL)
    {
        ngx_memzero(&r, sizeof(ngx_http_request_t));
        ngx_memzero(&status, sizeof(ngx_http_status_t));
        ngx_memzero(&body, sizeof(ngx_buf_t));
    }

    virtual ~ngx_dynamic_healthcheck_http()
    {
        if (pool != NULL)
            ngx_destroy_pool(pool);
    }
};


#endif /* NGX_DYNAMIC_HEALTHCHECK_HTTP_H */

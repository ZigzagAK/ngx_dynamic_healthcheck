/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

extern "C" {
#include <ngx_http.h>
}

#include <new>

#include "ngx_dynamic_healthcheck_api.h"
#include "ngx_dynamic_healthcheck_state.h"
#include "ngx_dynamic_healthcheck_tcp.h"
#include "ngx_dynamic_healthcheck_http.h"
#include "ngx_dynamic_healthcheck_ssl.h"


static void
ngx_dynamic_healthcheck_refresh_timers(ngx_event_t *ev)
{
    ngx_dynamic_healthcheck_api<ngx_http_upstream_main_conf_t,
        ngx_http_upstream_srv_conf_t>::refresh_timers(ev->log);
    ngx_dynamic_healthcheck_api<ngx_stream_upstream_main_conf_t,
        ngx_stream_upstream_srv_conf_t>::refresh_timers(ev->log);

    if (ngx_stopping())
        return;

    ngx_add_timer(ev, 1000);
}


ngx_int_t
ngx_dynamic_healthcheck_init_worker(ngx_cycle_t *cycle)
{
    if (ngx_process != NGX_PROCESS_WORKER && ngx_process != NGX_PROCESS_SINGLE)
        return NGX_OK;

    ngx_event_t *event = (ngx_event_t *) ngx_pcalloc(cycle->pool,
        sizeof(ngx_event_t));
    ngx_connection_t *dumb_conn = (ngx_connection_t *) ngx_pcalloc(cycle->pool,
        sizeof(ngx_connection_t));

    if (event == NULL || dumb_conn == NULL)
        return NGX_ERROR;

    dumb_conn->fd = -1;

    event->log = cycle->log;
    event->handler = ngx_dynamic_healthcheck_refresh_timers;
    event->data = dumb_conn;

    ngx_add_timer(event, 0);

    return NGX_OK;
}


template <class S, class PeersT, class PeerT> ngx_int_t
do_check_private(S *uscf, ngx_dynamic_healthcheck_event_t *event)
{
    PeerT                         *peer;
    PeersT                        *primary, *peers;
    ngx_uint_t                     i;
    void                          *addr;
    ngx_dynamic_hc_state_node_t    state;
    ngx_dynamic_healthcheck_peer  *p;
    ngx_str_t                      type = event->conf->shared->type;
    ngx_msec_t                     touched;
    ngx_addr_t                     saddr;
    ngx_pool_t                    *pool = NULL;
    ngx_str_t                      sname, host;

    ngx_str_null(&sname);
    ngx_str_null(&host);

    primary = (PeersT *) uscf->peer.data;
    peers = primary;

    ngx_rwlock_rlock(&primary->rwlock);

    if (event->conf->shared->port) {

        pool = ngx_create_pool(1024, event->log);
        if (pool == NULL)
            goto nomem;

        sname.data = (u_char *) ngx_pcalloc(pool, 1024);
        if (sname.data == NULL)
            goto nomem;

        ngx_log_debug3(NGX_LOG_DEBUG_HTTP, event->log, 0,
                       "[%V] %V: check port=%d",
                       &event->conf->shared->module,
                       &event->conf->shared->upstream,
                       event->conf->shared->port);
    }

    touched = ngx_current_msec;

    for (i = 0; peers && i < 2; peers = peers->next, i++) {

        for (peer = peers->peer; peer; peer = peer->next) {

            if (event->conf->shared->port) {

                ngx_memzero(&saddr, sizeof(ngx_addr_t));

                host = peer->name;
                for (; host.len > 0 && host.data[host.len - 1] != ':';
                       host.len--);

                host.len--;

                sname.len = ngx_snprintf(sname.data, 1024, "%V:%d", &host,
                    event->conf->shared->port) - sname.data;

                if (ngx_parse_addr_port(pool, &saddr, sname.data, sname.len)
                        == NGX_ERROR)
                    goto nomem;
            } else {

                saddr.sockaddr = peer->sockaddr;
                saddr.socklen = peer->socklen;
                saddr.name = peer->name;
            }

            state = ngx_dynamic_healthcheck_state_get(&event->conf->peers,
                        &peer->server, &peer->name,
                        saddr.sockaddr, saddr.socklen,
                        event->conf->shared->buffer_size);

            if (state.local == NULL)
                goto nomem;

            state.local->module = event->conf->config.module;
            state.local->upstream = event->conf->config.upstream;

            state.shared->down = peer->down;

            if (type.len == 3 && ngx_memcmp(type.data, "tcp", 3) == 0)

                addr = ngx_calloc(sizeof(ngx_dynamic_healthcheck_tcp<PeersT,
                                                                     PeerT>),
                                  event->log);

            else if (type.len == 4 && ngx_memcmp(type.data, "http", 4) == 0)

                addr = ngx_calloc(sizeof(ngx_dynamic_healthcheck_http<PeersT,
                                                                      PeerT>),
                                  event->log);

            else if (type.len == 3 && ngx_memcmp(type.data, "ssl", 3) == 0)

                addr = ngx_calloc(sizeof(ngx_dynamic_healthcheck_ssl<PeersT,
                                                                     PeerT>),
                                  event->log);
            else
                goto end;

            if (addr == NULL)
                goto nomem;

            if (type.len == 3 && ngx_memcmp(type.data, "tcp", 3) == 0)

                p = new (addr)
                    ngx_dynamic_healthcheck_tcp<PeersT, PeerT>(primary, event,
                        state);

            else if (type.len == 4 && ngx_memcmp(type.data, "http", 4) == 0)

                p = new (addr)
                    ngx_dynamic_healthcheck_http<PeersT, PeerT>(primary, event,
                        state);

            else if (type.len == 3 && ngx_memcmp(type.data, "ssl", 3) == 0)

                p = new (addr)
                    ngx_dynamic_healthcheck_ssl<PeersT, PeerT>(primary, event,
                        state);

            else
                continue;

            p->check();
        }

    }

    ngx_dynamic_healthcheck_state_gc(&event->conf->shared->state, touched);

end:

    ngx_rwlock_unlock(&primary->rwlock);

    if (pool != NULL)
        ngx_destroy_pool(pool);

    return NGX_OK;

nomem:

    ngx_rwlock_unlock(&primary->rwlock);

    if (pool != NULL)
        ngx_destroy_pool(pool);

    ngx_log_error(NGX_LOG_ERR, event->log, 0,
                  "[%V] no memory", &event->conf->config.module);

    return NGX_ERROR;
}


ngx_int_t
ngx_dynamic_event_handler_base::do_check(ngx_http_upstream_srv_conf_t *uscf,
    ngx_dynamic_healthcheck_event_t *event)
{
    return do_check_private<ngx_http_upstream_srv_conf_t,
                            ngx_http_upstream_rr_peers_t,
                            ngx_http_upstream_rr_peer_t>(uscf, event);
}


ngx_int_t
ngx_dynamic_event_handler_base::do_check(ngx_stream_upstream_srv_conf_t *uscf,
    ngx_dynamic_healthcheck_event_t *event)
{
    return do_check_private<ngx_stream_upstream_srv_conf_t,
                            ngx_stream_upstream_rr_peers_t,
                            ngx_stream_upstream_rr_peer_t>(uscf, event);
}

/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_PEER_H
#define NGX_DYNAMIC_HEALTHCHECK_PEER_H


extern "C" {
#include <ngx_core.h>
#include <ngx_event.h>
}

#include "ngx_dynamic_healthcheck.h"
#include "ngx_dynamic_healthcheck_state.h"


class ngx_dynamic_healthcheck_peer
{
private:

    ngx_dynamic_healthcheck_opts_t   *opts;
    ngx_dynamic_hc_state_node_t       state;

    typedef enum {
        st_none,
        st_connecting,
        st_connected,
        st_sending,
        st_sent,
        st_receiving,
        st_done
    } ngx_check_state_t;
    ngx_check_state_t                 check_state;
    
protected:

    ngx_str_t         name;
    ngx_str_t         server;
    ngx_str_t         upstream;
    ngx_str_t         module;

    ngx_dynamic_healthcheck_event_t  *event;

private:

    static void
    handle_idle(ngx_event_t *ev);

    static void
    handle_connect(ngx_event_t *ev);

    static void
    handle_write(ngx_event_t *ev);

    static void
    handle_read(ngx_event_t *ev);

    static void
    handle_dummy(ngx_event_t *ev);

    ngx_int_t
    handle_io(ngx_event_t *ev);

    void
    abort();

    void
    fail(ngx_flag_t skip = 0);

    void
    success();

    ngx_int_t
    peek();

    void
    close();

    void
    set_keepalive();

    void
    connect();

    void
    completed();

protected:

    virtual void
    up() = 0;

    virtual void
    down(ngx_flag_t skip = 0) = 0;

    virtual ngx_int_t
    on_send(ngx_dynamic_hc_local_node_t *state) = 0;

    virtual ngx_int_t
    on_recv(ngx_dynamic_hc_local_node_t *state) = 0;

public:

    ngx_dynamic_healthcheck_peer(ngx_dynamic_healthcheck_event_t *ev,
        ngx_dynamic_hc_state_node_t s);

    void
    check();

    virtual ~ngx_dynamic_healthcheck_peer();
};


template <class PeersT, class PeerT> class ngx_dynamic_healthcheck_peer_wrap :
    public ngx_dynamic_healthcheck_peer
{
    PeersT  *primary;

    PeerT *
    find_peer()
    {
        PeersT      *peers = primary;
        PeerT       *peer;
        ngx_uint_t   i;

        for (i = 0; peers && i < 2; peers = peers->next, i++) {

            for (peer = peers->peer; peer; peer = peer->next)

                if (ngx_memn2cmp(server.data, peer->server.data,
                                 server.len, peer->server.len) == 0
                    && ngx_memn2cmp(name.data, peer->name.data,
                                    name.len, peer->name.len) == 0)
                    return peer;
        }

        return NULL;
    }

protected:

    virtual void
    up()
    {
        ngx_rwlock_rlock(&primary->rwlock);

        PeerT  *peer = find_peer();

        if (peer != NULL) {

            ngx_rwlock_wlock(&peer->lock);

            if (peer->down) {
                peer->down = 0;
                ngx_log_error(NGX_LOG_NOTICE, event->log, 0,
                              "[%V] %V: %V addr=%V up",
                              &module, &upstream, &server, &name);
            }

            ngx_rwlock_unlock(&peer->lock);
        }

        ngx_rwlock_unlock(&primary->rwlock);
    }

    virtual void
    down(ngx_flag_t skip = 0)
    {
        ngx_rwlock_rlock(&primary->rwlock);

        PeerT  *peer = find_peer();

        if (peer != NULL) {

            ngx_rwlock_wlock(&peer->lock);

            if (!peer->down) {
                peer->down = 1;
                if (!skip) {
                    ngx_log_error(NGX_LOG_WARN, event->log, 0,
                                  "[%V] %V: %V addr=%V down",
                                  &module, &upstream, &server, &name);
                }
            }

            ngx_rwlock_unlock(&peer->lock);
        }

        ngx_rwlock_unlock(&primary->rwlock);
    }

public:

    ngx_dynamic_healthcheck_peer_wrap(PeersT *peers,
        ngx_dynamic_healthcheck_event_t *event, ngx_dynamic_hc_state_node_t s)
        : ngx_dynamic_healthcheck_peer(event, s), primary(peers)
    {
        name     = s.local->name;
        server   = s.local->server;
        upstream = s.local->upstream;
        module   = s.local->module;

        event->remains++;
    }

    virtual ~ngx_dynamic_healthcheck_peer_wrap()
    {
        event->remains--;
    }
};


ngx_int_t
ngx_dynamic_healthcheck_match_buffer(ngx_str_t *pattern, ngx_str_t *s);

#endif /* NGX_DYNAMIC_HEALTHCHECK_PEER_H */


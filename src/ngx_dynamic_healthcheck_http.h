/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_HTTP_H
#define NGX_DYNAMIC_HEALTHCHECK_HTTP_H


#include "ngx_dynamic_healthcheck_tcp.h"


class healthcheck_http_helper {

private:

    ngx_str_t  name;
    ngx_str_t  server;
    ngx_str_t  upstream;
    ngx_str_t  module;

    ngx_http_request_t  r;
    ngx_http_status_t   status;
    ssize_t             remains;
    ngx_int_t           content_length;
    ngx_flag_t          chunked;
    ngx_flag_t          eof;
    ngx_buf_t          *body;
    ngx_pool_t         *pool;

private:

    ngx_int_t receive_data(ngx_dynamic_hc_local_node_t *state);

    ngx_int_t parse_status_line(ngx_dynamic_hc_local_node_t *state);

    ngx_int_t parse_headers(ngx_dynamic_hc_local_node_t *state);

    ngx_int_t receive_headers(ngx_dynamic_healthcheck_opts_t *shared,
        ngx_dynamic_hc_local_node_t *state);

    ngx_int_t parse_body_chunked(ngx_dynamic_hc_local_node_t *state);

    ngx_int_t parse_body(ngx_dynamic_hc_local_node_t *state);

    ngx_int_t receive_body(ngx_dynamic_healthcheck_opts_t *shared,
        ngx_dynamic_hc_local_node_t *state);

public:

    healthcheck_http_helper(ngx_dynamic_hc_state_node_t s)
        : remains(0), content_length(-1), chunked(0), eof(0), pool(NULL)
    {
        name     = s.local->name;
        server   = s.local->server;
        upstream = s.local->upstream;
        module   = s.local->module;

        ngx_memzero(&r, sizeof(ngx_http_request_t));
        ngx_memzero(&status, sizeof(ngx_http_status_t));
    }

    ngx_int_t make_request(ngx_dynamic_healthcheck_opts_t *shared,
        ngx_dynamic_hc_local_node_t *state);

    ngx_int_t receive(ngx_dynamic_healthcheck_opts_t *shared,
        ngx_dynamic_hc_local_node_t *state);

    ~healthcheck_http_helper();
};


template <class PeersT, class PeerT> class ngx_dynamic_healthcheck_http :
    public ngx_dynamic_healthcheck_tcp<PeersT, PeerT>
{

    healthcheck_http_helper helper;

protected:

    
    virtual ngx_int_t
    on_send(ngx_dynamic_hc_local_node_t *state)
    {
        if (this->event->conf->shared->request_uri.len == 0)
            goto tcp;

        if (state->buf->last == state->buf->start)
            if (helper.make_request(this->shared, state)
                    == NGX_ERROR)
                return NGX_ERROR;

tcp:

        return ngx_dynamic_healthcheck_tcp<PeersT, PeerT>::on_send(state);
    }

    virtual ngx_int_t
    on_recv(ngx_dynamic_hc_local_node_t *state)
    {
        return helper.receive(this->shared, state);
    }
    
public:

    ngx_dynamic_healthcheck_http(PeersT *peers,
        ngx_dynamic_healthcheck_event_t *event, ngx_dynamic_hc_state_node_t s)
        : ngx_dynamic_healthcheck_tcp<PeersT, PeerT>(peers, event, s),
          helper(s)
    {}

    virtual ~ngx_dynamic_healthcheck_http()
    {}
};


#endif /* NGX_DYNAMIC_HEALTHCHECK_HTTP_H */

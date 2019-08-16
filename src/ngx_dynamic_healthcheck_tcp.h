/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_TCP_H
#define NGX_DYNAMIC_HEALTHCHECK_TCP_H


#include "ngx_dynamic_healthcheck_peer.h"


template <class PeersT, class PeerT> class ngx_dynamic_healthcheck_tcp :
    public ngx_dynamic_healthcheck_peer_wrap<PeersT, PeerT>
{
protected:

    ngx_dynamic_healthcheck_opts_t *shared;

    virtual ngx_int_t
    on_send(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_buf_t         *buf = state->buf;
        ngx_connection_t  *c = state->pc.connection;
        ssize_t            size;
#if (NGX_DEBUG)
        ngx_str_t          tmp;
#endif

        if (shared->request_body.len == 0 && buf->last == buf->start)
            return NGX_DECLINED;

        if (buf->last == buf->start)
            buf->last = ngx_snprintf(buf->last, shared->buffer_size,
                                     "%V", &shared->request_body);

        size = c->send(c, buf->pos, buf->last - buf->pos);

        if (size == NGX_ERROR)
            return NGX_ERROR;

        if (size == NGX_AGAIN)
            return NGX_AGAIN;

        buf->pos += size;

#if (NGX_DEBUG)
        if (buf->pos == buf->last) {
            tmp.data = buf->start;
            tmp.len = buf->last - buf->start;
            ngx_log_error(NGX_LOG_DEBUG, c->log, 0,
                          "[%V] %V: %V addr=%V, fd=%d %V request:\n%V",
                          &this->module, &this->upstream,
                          &this->server, &this->name, c->fd,
                          &shared->type, &tmp);
        }
#endif

        return buf->pos == buf->last ? NGX_OK : NGX_AGAIN;
    }

    virtual ngx_int_t
    on_recv(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_buf_t         *buf = state->buf;
        ngx_connection_t  *c = state->pc.connection;
        ssize_t            size;
        ngx_str_t          s;

        if (shared->response_body.len == 0)
            return NGX_DECLINED;

        size = c->recv(c, buf->last, buf->end - buf->last);

        ngx_log_error(NGX_LOG_DEBUG, c->log, 0,
                      "[%V] %V: %V addr=%V, "
                      "fd=%d on_recv() recv: %d",
                      &this->module, &this->upstream,
                      &this->server, &this->name, c->fd,
                      size);

        if (size == NGX_ERROR)
            return NGX_ERROR;

        if (size == NGX_AGAIN)
            return NGX_AGAIN;

        s.data = buf->last;
        s.len = size;

        ngx_log_error(NGX_LOG_DEBUG, c->log, 0,
                      "[%V] %V: %V addr=%V, fd=%d received:\n%V",
                      &this->module, &this->upstream,
                      &this->server, &this->name,
                      c->fd, &s);

        buf->last += size;

        s.data = buf->start;
        s.len = (size_t) (buf->last - buf->start);

        switch(ngx_dynamic_healthcheck_match_buffer(&shared->response_body,
                                                    &s)) {
            case NGX_OK:
                ngx_log_error(NGX_LOG_DEBUG, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d pattern '%V' found",
                              &this->module, &this->upstream,
                              &this->server, &this->name,
                              c->fd, &shared->response_body);
                return NGX_OK;

            case NGX_ERROR:
                ngx_log_error(NGX_LOG_DEBUG, c->log, 0,
                              "[%V] %V: %V addr=%V, fd=%d pattern '%V' error",
                              &this->module, &this->upstream,
                              &this->server, &this->name,
                              c->fd, &shared->response_body);
                return NGX_ERROR;

            case NGX_DECLINED:
            default:
                break;
        }

        if (buf->last == buf->end) {
            ngx_log_error(NGX_LOG_WARN, c->log, 0,
                          "[%V] %V: %V addr=%V, fd=%d pattern '%V' is not found"
                          "or 'healthcheck_buffer_size' is not enought",
                          &this->module, &this->upstream,
                          &this->server, &this->name, c->fd,
                          &shared->response_body);
            return NGX_ERROR;
        }

        if (c->read->eof)
            return NGX_ERROR;

        return NGX_AGAIN;
    }

public:

    ngx_dynamic_healthcheck_tcp(PeersT *peers,
        ngx_dynamic_healthcheck_event_t *event, ngx_dynamic_hc_state_node_t s)
        : ngx_dynamic_healthcheck_peer_wrap<PeersT, PeerT>(peers, event, s)
    {
        shared = event->conf->shared;
    }
};


#endif /* NGX_DYNAMIC_HEALTHCHECK_TCP_H */


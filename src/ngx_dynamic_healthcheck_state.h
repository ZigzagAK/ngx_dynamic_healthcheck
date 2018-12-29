/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_STATE_H
#define NGX_DYNAMIC_HEALTHCHECK_STATE_H


#ifdef __cplusplus
extern "C" {
#endif


#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_rbtree.h>


typedef struct {
    ngx_rbtree_t                   rbtree;
    ngx_rbtree_node_t              sentinel;
    ngx_slab_pool_t               *slab;
} ngx_dynamic_hc_shared_t;


typedef struct {
    ngx_rbtree_t                   rbtree;
    ngx_rbtree_node_t              sentinel;
} ngx_dynamic_hc_local_t;


typedef struct {
    ngx_dynamic_hc_shared_t       *shared;
    ngx_dynamic_hc_local_t         local;
} ngx_dynamic_hc_state_t;


typedef struct {
    ngx_str_node_t                 name;

    ngx_int_t                      fall;
    ngx_int_t                      rise;
    ngx_int_t                      fall_total;
    ngx_int_t                      rise_total;

    ngx_msec_t                     touched;
    time_t                         checked;

    ngx_dynamic_hc_shared_t       *state;
} ngx_dynamic_hc_shared_node_t;


typedef struct {
    ngx_str_node_t                 name;

    ngx_str_t                      module;
    ngx_str_t                      upstream;

    ngx_str_t                      server;
    struct sockaddr               *sockaddr;
    socklen_t                      socklen;

    ngx_peer_connection_t          pc;
    ngx_pool_t                    *pool;
    ngx_buf_t                     *buf;

    ngx_msec_t                     expired;

    ngx_dynamic_hc_local_t        *state;
} ngx_dynamic_hc_local_node_t;


typedef struct {
    ngx_dynamic_hc_shared_node_t  *shared;
    ngx_dynamic_hc_local_node_t   *local;
} ngx_dynamic_hc_state_node_t;


typedef struct ngx_dynamic_hc_stat_s {
    ngx_int_t                      fall;
    ngx_int_t                      rise;
    ngx_int_t                      fall_total;
    ngx_int_t                      rise_total;
} ngx_dynamic_hc_stat_t;


ngx_dynamic_hc_state_node_t
ngx_dynamic_healthcheck_state_get(ngx_dynamic_hc_state_t *state,
    ngx_str_t *server, ngx_str_t *name,
    struct sockaddr *sockaddr, socklen_t socklen, size_t buffer_size);


ngx_int_t
ngx_dynamic_healthcheck_state_stat(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name, ngx_dynamic_hc_stat_t *stat);

void
ngx_dynamic_healthcheck_state_delete(ngx_dynamic_hc_state_node_t state);


void
ngx_dynamic_healthcheck_state_gc(ngx_dynamic_hc_shared_t *state,
    ngx_msec_t touched);

void
ngx_dynamic_healthcheck_state_checked(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name);


#ifdef __cplusplus
}
#endif

#endif /* NGX_DYNAMIC_HEALTHCHECK_STATE_H */

/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#include "ngx_dynamic_healthcheck_state.h"


ngx_int_t
ngx_dynamic_healthcheck_state_stat(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name, ngx_dynamic_hc_stat_t *stat)
{
    ngx_dynamic_hc_shared_node_t  *shared;
    uint32_t                       hash;
    ngx_rbtree_t                  *rbtree = &state->shared->rbtree;
    ngx_slab_pool_t               *slab = state->shared->slab;

    hash = ngx_crc32_short(name->data, name->len);

    ngx_shmtx_lock(&slab->mutex);

    shared = (ngx_dynamic_hc_shared_node_t *)
        ngx_str_rbtree_lookup(rbtree, name, hash);

    if (shared == NULL) {

        ngx_shmtx_unlock(&slab->mutex);

        return NGX_DECLINED;
    }

    stat->fall = shared->fall;
    stat->rise = shared->rise;
    stat->fall_total = shared->fall_total;
    stat->rise_total = shared->rise_total;

    ngx_shmtx_unlock(&slab->mutex);

    return NGX_OK;
}


static ngx_dynamic_hc_local_node_t *
ngx_dynamic_healthcheck_create_local(ngx_str_t *server, ngx_str_t *name,
    size_t buffer_size, struct sockaddr *sockaddr, socklen_t socklen)
{
    ngx_pool_t                   *pool;
    ngx_dynamic_hc_local_node_t  *n;

    pool = ngx_create_pool(ngx_pagesize - 1, ngx_cycle->log);
    if (pool == NULL)
        return NULL;

    n = ngx_pcalloc(pool, sizeof(ngx_dynamic_hc_local_node_t));
    if (n == NULL)
        goto nomem;

    n->pool = pool;

    n->server.data = ngx_pcalloc(pool, server->len);
    if (n->server.data == NULL)
        goto nomem;

    ngx_memcpy(n->server.data, server->data, server->len);
    n->server.len = server->len;

    n->name.str.data = ngx_pcalloc(pool, name->len);
    if (n->name.str.data == NULL)
        goto nomem;

    ngx_memcpy(n->name.str.data, name->data, name->len);
    n->name.str.len = name->len;

    n->buf = ngx_create_temp_buf(pool, buffer_size);
    if (n->buf == NULL)
        goto nomem;

    n->sockaddr = ngx_pcalloc(pool, socklen);
    if (n->sockaddr == NULL)
        goto nomem;

    ngx_memcpy(n->sockaddr, sockaddr, socklen);
    n->socklen = socklen;

    return n;

nomem:

    ngx_destroy_pool(pool);
    return NULL;
}


ngx_dynamic_hc_state_node_t
ngx_dynamic_healthcheck_state_get(ngx_dynamic_hc_state_t *state,
    ngx_str_t *server, ngx_str_t *name,
    struct sockaddr *sockaddr, socklen_t socklen, size_t buffer_size)
{
    ngx_dynamic_hc_state_node_t   n;
    ngx_rbtree_node_t            *node;
    uint32_t                      hash;
    ngx_rbtree_t                 *local = &state->local.rbtree;
    ngx_rbtree_t                 *shared = &state->shared->rbtree;
    ngx_slab_pool_t              *slab = state->shared->slab;
    ngx_flag_t                    nomem = 0;

    hash = ngx_crc32_short(name->data, name->len);

    ngx_memzero(&n, sizeof(ngx_dynamic_hc_state_node_t));

    ngx_shmtx_lock(&slab->mutex);

    n.shared = (ngx_dynamic_hc_shared_node_t *)
        ngx_str_rbtree_lookup(shared, name, hash);

    if (n.shared != NULL) {

        n.local = (ngx_dynamic_hc_local_node_t *)
            ngx_str_rbtree_lookup(local, name, hash);

        if (n.local != NULL) {

            if (n.local->pc.connection == NULL) {

                ngx_rbtree_delete(local, (ngx_rbtree_node_t *) n.local);

                ngx_destroy_pool(n.local->pool);
                n.local = NULL;

            } else
                goto done;
        }

        n.local = ngx_dynamic_healthcheck_create_local(server, name,
                                                       buffer_size,
                                                       sockaddr, socklen);
        if (n.local == NULL)
            goto done;

        n.local->state = &state->local;

        node = (ngx_rbtree_node_t *) n.local;
        node->key = hash;

        ngx_rbtree_insert(local, node);

        goto done;
    }

    // alloc shared state

    n.shared = ngx_slab_calloc_locked(slab,
                                      sizeof(ngx_dynamic_hc_shared_node_t));
    if (n.shared == NULL)
        goto done;

    n.shared->name.str.data = ngx_slab_calloc_locked(slab, name->len);
    if (n.shared->name.str.data == NULL) {
        nomem = 1;
        goto done;
    }
    ngx_memcpy(n.shared->name.str.data, name->data, name->len);
    n.shared->name.str.len = name->len;

    n.shared->state = state->shared;

    // alloc worker local

    n.local = ngx_dynamic_healthcheck_create_local(server, name,
                                                   buffer_size,
                                                   sockaddr, socklen);
    if (n.local == NULL) {
        nomem = 1;
        goto done;
    }

    n.local->state = &state->local;

    // insert nodes
    
    node = (ngx_rbtree_node_t *) n.shared;
    node->key = hash;

    ngx_rbtree_insert(shared, node);

    node = (ngx_rbtree_node_t *) n.local;
    node->key = hash;

    ngx_rbtree_insert(local, node);

done:

    if (nomem) {

        if (n.shared->name.str.data != NULL)
            ngx_slab_free_locked(slab, n.shared->name.str.data);

        ngx_slab_free_locked(slab, n.shared);

        if (n.local != NULL)
            ngx_destroy_pool(n.local->pool);

        n.shared = NULL;
        n.local = NULL;
    }

    if (n.shared != NULL && n.local != NULL)
        n.shared->touched = ngx_current_msec;

    ngx_shmtx_unlock(&slab->mutex);

    return n;
}


void
ngx_dynamic_healthcheck_state_delete(ngx_dynamic_hc_state_node_t state)
{
    ngx_slab_pool_t  *slab = state.shared->state->slab;

    ngx_shmtx_lock(&slab->mutex);

    if (state.local != NULL) {

        ngx_rbtree_delete(&state.local->state->rbtree,
            (ngx_rbtree_node_t *) state.local);
        ngx_destroy_pool(state.local->pool);
    }

    ngx_rbtree_delete(&state.shared->state->rbtree,
        (ngx_rbtree_node_t *) state.shared);

    ngx_slab_free_locked(slab, state.shared->name.str.data);

    ngx_shmtx_unlock(&slab->mutex);

    ngx_slab_free(slab, state.shared);
}


void
ngx_dynamic_healthcheck_state_gc(ngx_dynamic_hc_shared_t *state,
    ngx_msec_t touched)
{
    ngx_dynamic_hc_shared_node_t  *n;
    ngx_rbtree_node_t             *node, *root, *sentinel;
    ngx_slab_pool_t               *slab = state->slab;
    ngx_dynamic_hc_state_node_t    del;

    del.local = NULL;

again:

    ngx_shmtx_lock(&slab->mutex);

    sentinel = state->rbtree.sentinel;
    root = state->rbtree.root;

    if (root == sentinel) {
        ngx_shmtx_unlock(&slab->mutex);
        return;
    }

    for (node = ngx_rbtree_min(root, sentinel);
         node;
         node = ngx_rbtree_next(&state->rbtree, node))
    {
        n = (ngx_dynamic_hc_shared_node_t *) node;

        if (n->touched < touched) {
            ngx_shmtx_unlock(&slab->mutex);
            del.shared = n;
            ngx_dynamic_healthcheck_state_delete(del);
            goto again;
        }
    }

    ngx_shmtx_unlock(&slab->mutex);
}


void
ngx_dynamic_healthcheck_state_checked(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name)
{
    ngx_dynamic_hc_shared_node_t  *shared;
    uint32_t                       hash;
    ngx_rbtree_t                  *rbtree = &state->shared->rbtree;
    ngx_slab_pool_t               *slab = state->shared->slab;
    ngx_time_t                    *tp = ngx_timeofday();

    hash = ngx_crc32_short(name->data, name->len);

    ngx_shmtx_lock(&slab->mutex);

    shared = (ngx_dynamic_hc_shared_node_t *)
        ngx_str_rbtree_lookup(rbtree, name, hash);

    if (shared != NULL)
        shared->checked = tp->sec;

    ngx_shmtx_unlock(&slab->mutex);
}

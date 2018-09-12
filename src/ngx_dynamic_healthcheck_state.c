/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#include "ngx_dynamic_healthcheck_state.h"


ngx_int_t
ngx_dynamic_healthcheck_state_stat(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name, ngx_dynamic_hc_stat_t *stat)
{
    ngx_dynamic_hc_state_node_t  *n;
    uint32_t                      hash;
    ngx_rbtree_t                 *rbtree = &state->rbtree;
    ngx_slab_pool_t              *slab = state->slab;

    hash = ngx_crc32_short(name->data, name->len);

    ngx_shmtx_lock(&slab->mutex);

    n = (ngx_dynamic_hc_state_node_t *)
        ngx_str_rbtree_lookup(rbtree, name, hash);

    if (n != NULL) {

        stat->fall = n->fall;
        stat->fall_total = n->fall_total;
        stat->rise = n->rise;
        stat->rise_total = n->rise_total;
        stat->touched = n->touched;

        ngx_shmtx_unlock(&slab->mutex);

        return NGX_OK;
    }

    ngx_shmtx_unlock(&slab->mutex);

    return NGX_DECLINED;
}


ngx_dynamic_hc_state_node_t *
ngx_dynamic_healthcheck_state_get(ngx_dynamic_hc_state_t *state,
    ngx_str_t *name, struct sockaddr *sockaddr, socklen_t socklen,
    size_t buffer_size)
{
    ngx_dynamic_hc_state_node_t  *n;
    ngx_rbtree_node_t            *node;
    uint32_t                      hash;
    ngx_rbtree_t                 *rbtree = &state->rbtree;
    ngx_slab_pool_t              *slab = state->slab;
    ngx_flag_t                    nomem = 0;
    void                         *p;

    hash = ngx_crc32_short(name->data, name->len);

    ngx_shmtx_lock(&slab->mutex);

    n = (ngx_dynamic_hc_state_node_t *)
        ngx_str_rbtree_lookup(rbtree, name, hash);

    if (n != NULL) {
        if (n->pid == state->pid && n->generation == state->generation) {
            ngx_log_debug4(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                           "[%V] %V: addr=%V reuse state fd=%d",
                           &n->module, &n->upstream, &n->name.str,
                           n->pc.connection ? n->pc.connection->fd : -1);
            goto done;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                       "%V recreate state", name);

        n->pool = ngx_create_pool(ngx_pagesize - 1, ngx_cycle->log);
        if (n->pool == NULL) {
            nomem = 1;
            goto done;
        }

        p = ngx_pnalloc(n->pool, buffer_size);
        if (p == NULL) {
            nomem = 1;
            goto done;
        }

        ngx_memzero(&n->buf, sizeof(ngx_buf_t));

        n->buf.start = p;
        n->buf.end = (u_char *) p + buffer_size;
        n->buf.pos = p;
        n->buf.last = p;

        n->pid = state->pid;
        n->generation = state->generation;

        n->pc.connection = NULL;

        goto done;
    }

    n = ngx_slab_calloc_locked(slab, sizeof(ngx_dynamic_hc_state_node_t));
    if (n == NULL)
        goto done;

    n->name.str.data = ngx_slab_calloc_locked(slab, name->len);
    if (n == NULL) {
        nomem = 1;
        goto done;
    }
    ngx_memcpy(n->name.str.data, name->data, name->len);
    n->name.str.len = name->len;

    n->pid = state->pid;
    n->generation = state->generation;

    n->sockaddr = ngx_slab_calloc_locked(slab, socklen);
    if (n->sockaddr == NULL) {
        nomem = 1;
        goto done;
    }
    ngx_memcpy(n->sockaddr, sockaddr, socklen);
    n->socklen = socklen;

    n->pool = ngx_create_pool(ngx_pagesize - 1, ngx_cycle->log);
    if (n->pool == NULL) {
        nomem = 1;
        goto done;
    }

    p = ngx_pnalloc(n->pool, buffer_size);
    if (p == NULL) {
        nomem = 1;
        goto done;
    }

    n->buf.start = p;
    n->buf.end = (u_char *) p + buffer_size;
    n->buf.pos = p;
    n->buf.last = p;

    n->state = state;
    
    node = (ngx_rbtree_node_t *) n;
    node->key = hash;

    ngx_rbtree_insert(rbtree, node);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "%V new state", name);

done:

    if (nomem) {
        if (n->name.str.data != NULL)
            ngx_slab_free_locked(slab, n->name.str.data);
        if (n->sockaddr != NULL)
            ngx_slab_free_locked(slab, n->sockaddr);
        if (n->pool)
            ngx_destroy_pool(n->pool);
        ngx_slab_free_locked(slab, n);
        n = NULL;
    } else
        n->touched = ngx_current_msec;

    ngx_shmtx_unlock(&slab->mutex);

    return n;
}


void
ngx_dynamic_healthcheck_state_delete(ngx_dynamic_hc_state_node_t *state)
{
    ngx_slab_pool_t *slab = state->state->slab;

    ngx_shmtx_lock(&slab->mutex);

    ngx_log_debug3(NGX_LOG_DEBUG_HTTP, ngx_cycle->log, 0,
                   "[%V] %V: addr=%V delete state",
                   &state->module, &state->upstream, &state->name.str);

    ngx_rbtree_delete(&state->state->rbtree, (ngx_rbtree_node_t *) state);

    ngx_slab_free_locked(slab, state->name.str.data);
    ngx_slab_free_locked(slab, state->sockaddr);

    if (state->pid == state->state->pid
        && state->generation == state->state->generation)
        ngx_destroy_pool(state->pool);

    ngx_shmtx_unlock(&slab->mutex);

    ngx_slab_free(slab, state);
}


void
ngx_dynamic_healthcheck_state_gc(ngx_dynamic_hc_state_t *state,
    ngx_msec_t touched)
{
    ngx_dynamic_hc_state_node_t  *n;
    ngx_rbtree_node_t            *node, *root, *sentinel;
    ngx_slab_pool_t              *slab = state->slab;

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
        n = (ngx_dynamic_hc_state_node_t *) node;
        if (n->touched < touched) {
            ngx_shmtx_unlock(&slab->mutex);
            ngx_dynamic_healthcheck_state_delete(n);
            goto again;
        }
    }

    ngx_shmtx_unlock(&slab->mutex);
}

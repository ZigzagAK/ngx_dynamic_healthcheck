/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_healthcheck.h"


static void
ngx_shm_free_safe(ngx_slab_pool_t *pool, void **p)
{
    if (*p != NULL) {
        ngx_slab_free_locked(pool, *p);
        *p = NULL;
    }
}


void
ngx_shm_str_free(ngx_str_t *src, ngx_slab_pool_t *slab)
{
    if (src->data != NULL)
        ngx_slab_free_locked(slab, src->data);

    ngx_str_null(src);
}


ngx_int_t
ngx_shm_str_copy(ngx_str_t *dst, ngx_str_t *src, ngx_slab_pool_t *slab)
{
    if (dst->data != NULL)
        ngx_slab_free_locked(slab, dst->data);

    ngx_str_null(dst);

    if (src->len == 0)
        return NGX_OK;

    dst->data = ngx_slab_calloc_locked(slab, src->len + 1);
    if (dst->data == NULL)
        return NGX_ERROR;

    ngx_memcpy(dst->data, src->data, src->len);
    dst->len = src->len;

    return NGX_OK;
}


void
ngx_shm_num_array_free(ngx_num_array_t *src, ngx_slab_pool_t *slab)
{
    if (src->data == NULL)
        return;

    ngx_slab_free_locked(slab, src->data);

    src->data = NULL;
    src->len = 0;
    src->reserved = 0;
}


ngx_int_t
ngx_shm_num_array_copy(ngx_num_array_t *dst, ngx_num_array_t *src,
    ngx_slab_pool_t *slab)
{
    if (src->len == 0) {
        ngx_memzero(dst->data, dst->len * sizeof(ngx_int_t));
        dst->len = 0;
        return NGX_OK;
    }

    if (dst->reserved < src->len) {
        ngx_shm_num_array_free(dst, slab);

        dst->data = ngx_slab_calloc_locked(slab,
                                          src->reserved * sizeof(ngx_int_t));
        if (dst->data == NULL)
           return NGX_ERROR;

        dst->reserved = src->reserved;
    } else
        ngx_memzero(dst->data, dst->len * sizeof(ngx_int_t));

    dst->len = src->len;

    ngx_memcpy(dst->data, src->data, sizeof(ngx_int_t) * dst->len);

    return NGX_OK;
}


ngx_int_t
ngx_shm_num_array_create(ngx_num_array_t *src, ngx_uint_t size,
    ngx_slab_pool_t *slab)
{
    src->data = ngx_slab_calloc_locked(slab, size * sizeof(ngx_int_t));
    if (src->data == NULL)
       return NGX_ERROR;

    src->reserved = size;
    src->len = 0;

    return NGX_OK;
}


void
ngx_shm_str_array_free(ngx_str_array_t *src, ngx_slab_pool_t *slab)
{
    ngx_uint_t i;

    if (src->data == NULL)
        return;

    for (i = 0; i < src->len && src->data[i].data; i++)
        ngx_slab_free_locked(slab, src->data[i].data);

    ngx_slab_free_locked(slab, src->data);

    src->data = NULL;
    src->len = 0;
    src->reserved = 0;
}


ngx_int_t
ngx_shm_str_array_create(ngx_str_array_t *src, ngx_uint_t size,
    ngx_slab_pool_t *slab)
{
    src->data = ngx_slab_calloc_locked(slab, size * sizeof(ngx_str_t));
    if (src->data == NULL)
       return NGX_ERROR;

    src->reserved = size;
    src->len = 0;

    return NGX_OK;
}


ngx_int_t
ngx_shm_str_array_copy(ngx_str_array_t *dst, ngx_str_array_t *src,
    ngx_slab_pool_t *slab)
{
    ngx_uint_t i;

    if (src->len == 0) {
        ngx_memzero(dst->data, dst->len * sizeof(ngx_str_t));
        dst->len = 0;
        return NGX_OK;
    }

    if (dst->reserved < src->len) {
        ngx_shm_str_array_free(dst, slab);

        dst->data = ngx_slab_calloc_locked(slab,
                                           src->reserved * sizeof(ngx_str_t));
        if (dst->data == NULL)
           return NGX_ERROR;

        dst->reserved = src->reserved;
    } else
        ngx_memzero(dst->data, dst->len * sizeof(ngx_str_t));

    dst->len = src->len;

    for (i = 0; i < src->len; i++) {
        dst->data[i].data = ngx_slab_calloc_locked(slab,
            src->data[i].len + 1);
        if (dst->data[i].data == NULL)
            goto nomem;
        dst->data[i].len = src->data[i].len;
        ngx_memcpy(dst->data[i].data, src->data[i].data, dst->data[i].len);
    }

    return NGX_OK;

nomem:

    for (i = 0; i < dst->len && dst->data[i].data != NULL; i++)
        ngx_slab_free_locked(slab, dst->data[i].data);

    ngx_slab_free_locked(slab, dst->data);

    dst->data = NULL;
    dst->len = 0;
    dst->reserved = 0;

    return NGX_ERROR;
}


void
ngx_shm_keyval_array_free(ngx_keyval_array_t *src, ngx_slab_pool_t *slab)
{
    ngx_uint_t i;

    if (src->data == NULL)
        return;

    for (i = 0; i < src->len && src->data[i].key.data != NULL; i++) {
        ngx_shm_free_safe(slab, (void **) &src->data[i].key.data);
        ngx_shm_free_safe(slab, (void **) &src->data[i].value.data);
    }

    ngx_slab_free_locked(slab, src->data);

    src->data = NULL;
    src->len = 0;
    src->reserved = 0;
}


ngx_int_t
ngx_shm_keyval_array_create(ngx_keyval_array_t *src, ngx_uint_t size,
                            ngx_slab_pool_t *slab)
{
    src->data = ngx_slab_calloc_locked(slab, size * sizeof(ngx_keyval_t));
    if (src->data == NULL)
       return NGX_ERROR;

    src->reserved = size;
    src->len = 0;

    return NGX_OK;
}


static ngx_int_t
ngx_shm_keyval_copy(ngx_keyval_t *dst, ngx_keyval_t *src, ngx_slab_pool_t *slab)
{
    if (ngx_shm_str_copy(&dst->key, &src->key, slab) == NGX_OK)
        if (ngx_shm_str_copy(&dst->value, &src->value, slab) == NGX_OK)
            return NGX_OK;
    return NGX_ERROR;
}


ngx_int_t
ngx_shm_keyval_array_copy(ngx_keyval_array_t *dst, ngx_keyval_array_t *src,
    ngx_slab_pool_t *slab)
{
    ngx_uint_t i;

    if (src->len == 0) {
        ngx_memzero(dst->data, dst->len * sizeof(ngx_keyval_t));
        dst->len = 0;
        return NGX_OK;
    }

    if (dst->reserved < src->len) {
        ngx_shm_keyval_array_free(dst, slab);

        dst->data = ngx_slab_calloc_locked(slab,
                                          src->reserved * sizeof(ngx_keyval_t));
        if (dst->data == NULL)
           return NGX_ERROR;

        dst->reserved = src->reserved;
    } else
        ngx_memzero(dst->data, dst->len * sizeof(ngx_keyval_t));

    dst->len = src->len;

    for (i = 0; i < src->len; i++)
        if (ngx_shm_keyval_copy(dst->data + i, src->data + i, slab)
                == NGX_ERROR)
            goto nomem;

    return NGX_OK;

nomem:

    for (i = 0; i < dst->len && dst->data[i].key.data != NULL; i++) {
        ngx_shm_free_safe(slab, (void **) &dst->data[i].key.data);
        ngx_shm_free_safe(slab, (void **) &dst->data[i].value.data);
    }

    ngx_slab_free_locked(slab, dst->data);
    dst->data = NULL;

    return NGX_ERROR;
}


typedef struct {
    ngx_dynamic_healthcheck_opts_t  opts;
    ngx_int_t                       count;
    ngx_queue_t                     queue;
} ngx_healthcehck_conf_t;


static ngx_healthcehck_conf_t *
ngx_shm_conf_get(ngx_str_t upstream, ngx_queue_t *sh,
    ngx_slab_pool_t *slab)
{
    ngx_queue_t             *q;
    ngx_healthcehck_conf_t  *conf;

    for (q = ngx_queue_head(sh);
         q != ngx_queue_sentinel(sh);
         q = ngx_queue_next(q))
    {
        conf = ngx_queue_data(q, ngx_healthcehck_conf_t, queue);
        if (conf->count >= 0
            && ngx_memn2cmp(upstream.data, conf->opts.upstream.data,
                           upstream.len, conf->opts.upstream.len) == 0) {
            conf->count++;
            return conf;
        }
    }

    conf = ngx_slab_calloc_locked(slab, sizeof(ngx_healthcehck_conf_t));
    if (conf != NULL) {
        ngx_queue_insert_tail(sh, &conf->queue);
        conf->count = 1;
        return conf;
    }

    return NULL;
}


static void
ngx_shm_conf_gc(ngx_queue_t *sh, ngx_slab_pool_t *slab)
{
    ngx_queue_t             *q;
    ngx_healthcehck_conf_t  *conf;

again:

    for (q = ngx_queue_head(sh);
         q != ngx_queue_sentinel(sh);
         q = ngx_queue_next(q))
    {
        conf = ngx_queue_data(q, ngx_healthcehck_conf_t, queue);
        if (conf->count > -4)
            continue;

        // delayed cleanup

        ngx_queue_remove(q);

        ngx_shm_str_free(&conf->opts.upstream, slab);
        ngx_shm_str_free(&conf->opts.module, slab);
        ngx_shm_str_free(&conf->opts.type, slab);
        ngx_shm_str_free(&conf->opts.request_uri, slab);
        ngx_shm_str_free(&conf->opts.request_method, slab);
        ngx_shm_str_free(&conf->opts.request_body, slab);
        ngx_shm_str_free(&conf->opts.response_body, slab);
        ngx_shm_num_array_free(&conf->opts.response_codes, slab);
        ngx_shm_keyval_array_free(&conf->opts.request_headers, slab);
        ngx_shm_str_array_free(&conf->opts.disabled_hosts, slab);
        ngx_shm_str_array_free(&conf->opts.disabled_hosts_manual, slab);
        ngx_shm_str_array_free(&conf->opts.excluded_hosts, slab);
        ngx_shm_str_array_free(&conf->opts.disabled_hosts_global, slab);

        ngx_dynamic_healthcheck_state_free(&conf->opts.state);

        ngx_slab_free_locked(slab, conf);

        goto again;
    }

    for (q = ngx_queue_head(sh);
         q != ngx_queue_sentinel(sh);
         q = ngx_queue_next(q))
    {
        conf = ngx_queue_data(q, ngx_healthcehck_conf_t, queue);
        conf->count--;
    }
}


static ngx_int_t
ngx_shm_conf_init(ngx_dynamic_healthcheck_conf_t *conf,
    ngx_dynamic_healthcheck_opts_t *sh, ngx_shm_zone_t *zone)
{
    ngx_dynamic_healthcheck_opts_t  *opts;
    ngx_slab_pool_t                 *slab;
    ngx_flag_t                       b = 1;

    slab = (ngx_slab_pool_t *) zone->shm.addr;

    opts = &conf->config;

    conf->zone = zone;

    if (sh->upstream.data == NULL) {
        ngx_rbtree_init(&sh->state.rbtree, &sh->state.sentinel,
                        ngx_str_rbtree_insert_value);

        if (ngx_shm_str_array_create(&sh->disabled_hosts_manual, 10, slab)
                == NGX_ERROR)
            return NGX_ERROR;

        b = NGX_OK == ngx_shm_str_copy(&sh->upstream, &opts->upstream, slab);
        b = b && NGX_OK == ngx_shm_str_copy(&sh->module, &opts->module, slab);
    }

    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_OFF))
        sh->off = opts->off;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_DISABLED))
        sh->disabled = opts->disabled;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_FALL))
        sh->fall = opts->fall;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_RISE))
        sh->rise = opts->rise;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_TIMEOUT))
        sh->timeout = opts->timeout;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_INTERVAL))
        sh->interval = opts->interval;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_KEEPALIVE))
        sh->keepalive = opts->keepalive;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_PORT))
        sh->port = opts->port;
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_PASSIVE))
        sh->passive = opts->passive;

    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_TYPE))
        b = b && NGX_OK == ngx_shm_str_copy(&sh->type, &opts->type, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_URI))
        b = b && NGX_OK == ngx_shm_str_copy(&sh->request_uri,
                                            &opts->request_uri, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_METHOD))
        b = b && NGX_OK == ngx_shm_str_copy(&sh->request_method,
                                            &opts->request_method, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_BODY))
        b = b && NGX_OK == ngx_shm_str_copy(&sh->request_body,
                                            &opts->request_body, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_BODY))
        b = b && NGX_OK == ngx_shm_str_copy(&sh->response_body,
                                            &opts->response_body, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_CODES))
        b = b && NGX_OK == ngx_shm_num_array_copy(&sh->response_codes,
                                                  &opts->response_codes, slab);
    if (!(sh->flags & NGX_DYNAMIC_UPDATE_OPT_HEADERS))
        b = b && NGX_OK == ngx_shm_keyval_array_copy(&sh->request_headers,
                                                     &opts->request_headers,
                                                     slab);

    b = b && NGX_OK == ngx_shm_str_array_copy(&sh->disabled_hosts,
                                              &opts->disabled_hosts, slab);
    b = b && NGX_OK == ngx_shm_str_array_copy(&sh->excluded_hosts,
                                              &opts->excluded_hosts, slab);
    b = b && NGX_OK == ngx_shm_str_array_copy(&sh->disabled_hosts_global,
                                              &opts->disabled_hosts_global,
                                              slab);

    conf->peers.shared = &sh->state;

    ngx_rbtree_init(&conf->peers.local.rbtree, &conf->peers.local.sentinel,
                    ngx_str_rbtree_insert_value);

    sh->state.slab = slab;
    sh->buffer_size = opts->buffer_size;

    sh->updated = 1;

    if (!b)
        return NGX_ERROR;

    return NGX_OK;
}


static ngx_int_t
ngx_init_shm_zone(ngx_shm_zone_t *zone, void *old)
{
    ngx_array_t                     *pconf;
    ngx_dynamic_healthcheck_conf_t  *conf;
    ngx_healthcehck_conf_t          *sh;
    ngx_queue_t                     *qsh;
    ngx_slab_pool_t                 *slab;
    ngx_uint_t                       j;

    pconf = zone->data;
    slab = (ngx_slab_pool_t *) zone->shm.addr;

    ngx_shmtx_lock(&slab->mutex);

    if (old) {
        qsh = slab->data;
    } else {
        qsh = ngx_slab_calloc_locked(slab, sizeof(ngx_queue_t));
        if (qsh == NULL) {
            ngx_shmtx_unlock(&slab->mutex);
            return NGX_ERROR;
        }
        ngx_queue_init(qsh);
        slab->data = qsh;
    }

    for (j = 0; j < pconf->nelts; j++) {

        conf = ((ngx_dynamic_healthcheck_conf_t **) pconf->elts)[j];

        sh = ngx_shm_conf_get(conf->config.upstream, qsh, slab);
        if (sh->opts.upstream.data == NULL)
            conf->post_init = NULL;

        if (ngx_shm_conf_init(conf, &sh->opts, zone) == NGX_ERROR) {
            ngx_shmtx_unlock(&slab->mutex);
            return NGX_ERROR;
        }

        conf->shared = &sh->opts;
    }

    ngx_shm_conf_gc(qsh, slab);

    ngx_shmtx_unlock(&slab->mutex);

    for (j = 0; j < pconf->nelts; j++) {

        conf = ((ngx_dynamic_healthcheck_conf_t **) pconf->elts)[j];
        if (conf->post_init)
            conf->post_init(conf);
    }

    return NGX_OK;
}


static ngx_shm_zone_t *
ngx_add_shm_zone(ngx_conf_t *cf, ngx_str_t mod, size_t size, void *tag)
{
    return ngx_shared_memory_add(cf, &mod, size, tag);
}


ngx_shm_zone_t *
ngx_shm_create_zone(ngx_conf_t *cf, ngx_dynamic_healthcheck_conf_t *conf,
    void *tag)
{
    ngx_shm_zone_t                   *zone;
    ngx_array_t                      *a;
    ngx_dynamic_healthcheck_conf_t  **pconf;

    zone = ngx_add_shm_zone(cf, conf->config.module, conf->zone_size, tag);
    if (zone == NULL)
        return NULL;

    zone->init = ngx_init_shm_zone;
    zone->noreuse = 0;

    if (zone->data == NULL) {
        a = ngx_array_create(cf->pool, 10, sizeof(void *));
        if (a == NULL)
            return NULL;
        zone->data = a;
    } else
        a = zone->data;

    if (conf->zone_size != 0)
        return zone;

    pconf = ngx_array_push(a);
    if (pconf == NULL)
        return NULL;

    *pconf = conf;

    return zone;
}


ngx_int_t
ngx_shm_add_to_zone(ngx_dynamic_healthcheck_conf_t *conf, ngx_shm_zone_t *zone)
{
    ngx_queue_t             *qsh;
    ngx_healthcehck_conf_t  *sh;
    ngx_slab_pool_t         *slab;

    slab = (ngx_slab_pool_t *) zone->shm.addr;
    qsh = slab->data;

    ngx_shmtx_lock(&slab->mutex);

    sh = ngx_shm_conf_get(conf->config.upstream, qsh, slab);
    if (sh->opts.upstream.data == NULL)
        conf->post_init = NULL;

    if (ngx_shm_conf_init(conf, &sh->opts, zone) == NGX_ERROR) {
        ngx_shmtx_unlock(&slab->mutex);
        return NGX_ERROR;
    }

    conf->shared = &sh->opts;
    conf->zone = zone;

    ngx_shmtx_unlock(&slab->mutex);

    if (conf->post_init != NULL)
        conf->post_init(conf);

    return NGX_OK;
}


#ifdef _WITH_LUA_API

ngx_int_t
lua_get_shm_string(lua_State *L, ngx_str_t *str,
    ngx_slab_pool_t *slab, int index)
{
    const char *s = lua_tostring(L, index);
    ngx_str_null(str);
    if (s != NULL) {
        str->len = strlen(s);
        str->data = ngx_slab_calloc_locked(slab, str->len + 1);
        if (str->data != NULL)
            ngx_memcpy(str->data, s, str->len);
        else
            str->len = 0;
    }
    return str->data != NULL ? NGX_OK : NGX_ERROR;
}

#endif

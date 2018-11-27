/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#include "ngx_dynamic_shm.h"


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


ngx_inline ngx_int_t
ngx_init_shm_zone(ngx_shm_zone_t *zone, void *old)
{
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_dynamic_healthcheck_opts_t *sh, *opts;
    ngx_flag_t                      b = 1;
    ngx_slab_pool_t                *slab;
    
    conf = (ngx_dynamic_healthcheck_conf_t *) zone->data;
    opts = &conf->config;

    conf->zone = zone;
    slab = (ngx_slab_pool_t *) zone->shm.addr;

    ngx_shmtx_lock(&slab->mutex);
    
    if (old != NULL) {
        sh = (ngx_dynamic_healthcheck_opts_t *) slab->data;
        if (opts->persistent.len != 0)
            goto skip;
    } else {
        sh = ngx_slab_calloc_locked(slab,
            sizeof(ngx_dynamic_healthcheck_opts_t));
        if (sh == NULL) {
            ngx_shmtx_unlock(&slab->mutex);
            return NGX_ERROR;
        }

        slab->data = sh;

        ngx_rbtree_init(&sh->state.rbtree, &sh->state.sentinel,
                        ngx_str_rbtree_insert_value);
    }

    sh->off       = opts->off;
    sh->disabled  = opts->disabled;
    sh->fall      = opts->fall;
    sh->rise      = opts->rise;
    sh->timeout   = opts->timeout;
    sh->interval  = opts->interval;
    sh->keepalive = opts->keepalive;
    sh->updated   = 1;

    b = b && NGX_OK == ngx_shm_str_copy(&sh->module, &opts->module, slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->upstream, &opts->upstream, slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->type, &opts->type, slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->request_uri, &opts->request_uri,
                                        slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->request_method,
                                        &opts->request_method, slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->request_body, &opts->request_body,
                                        slab);
    b = b && NGX_OK == ngx_shm_str_copy(&sh->response_body,
                                        &opts->response_body, slab);
    b = b && NGX_OK == ngx_shm_num_array_copy(&sh->response_codes,
                                              &opts->response_codes, slab);
    b = b && NGX_OK == ngx_shm_keyval_array_copy(&sh->request_headers,
                                                 &opts->request_headers, slab);
    b = b && NGX_OK == ngx_shm_str_array_copy(&sh->disabled_hosts,
                                              &opts->disabled_hosts, slab);

skip:

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

    ngx_shmtx_unlock(&slab->mutex);

    if (!b)
        return NGX_ERROR;

    conf->shared = sh;

    if (old != NULL)
        conf->post_init(conf);

    return NGX_OK;
}


ngx_shm_zone_t *
ngx_add_shm_zone(ngx_conf_t *cf, const u_char *mod,
    ngx_str_t *upstream, void *tag)
{
    ngx_str_t name;

    name.len = ngx_strlen(mod) + upstream->len + 1;
    name.data = ngx_pcalloc(cf->pool, name.len + 1);

    if (name.data == NULL)
        return NULL;

    ngx_snprintf(name.data, name.len + 1, "%s:%V", mod, upstream);

    return ngx_shared_memory_add(cf, &name,  262144, tag);
}


ngx_shm_zone_t *
ngx_shm_create_zone(ngx_conf_t *cf, ngx_dynamic_healthcheck_conf_t *conf,
    void *tag)
{
    ngx_shm_zone_t *zone;

    zone = ngx_add_shm_zone(cf, conf->config.module.data,
                            &conf->config.upstream, tag);

    if (zone == NULL)
        return NULL;

    zone->init = ngx_init_shm_zone;
    zone->noreuse = 0;
    zone->data = (void *) conf;

    return zone;
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

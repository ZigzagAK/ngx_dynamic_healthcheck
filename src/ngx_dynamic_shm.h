/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_SHM_H
#define NGX_DYNAMIC_SHM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ngx_core.h>

#ifdef _WITH_LUA_API
#include <lauxlib.h>
#endif

#ifdef __cplusplus
}
#endif

#include "ngx_dynamic_healthcheck.h"

#ifdef __cplusplus
extern "C" {
#endif

void
ngx_shm_str_free(ngx_str_t *src, ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_str_copy(ngx_str_t *dst, ngx_str_t *src, ngx_slab_pool_t *slab);

void
ngx_shm_num_array_free(ngx_num_array_t *src, ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_num_array_copy(ngx_num_array_t *dst, ngx_num_array_t *src,
    ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_num_array_create(ngx_num_array_t *src, ngx_uint_t size,
    ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_str_array_copy(ngx_str_array_t *dst, ngx_str_array_t *src,
    ngx_slab_pool_t *slab);

void
ngx_shm_str_array_free(ngx_str_array_t *src, ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_str_array_create(ngx_str_array_t *src, ngx_uint_t size,
    ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_keyval_array_copy(ngx_keyval_array_t *dst, ngx_keyval_array_t *src,
    ngx_slab_pool_t *slab);

void
ngx_shm_keyval_array_free(ngx_keyval_array_t *src, ngx_slab_pool_t *slab);

ngx_int_t
ngx_shm_keyval_array_create(ngx_keyval_array_t *src, ngx_uint_t size,
    ngx_slab_pool_t *slab);

ngx_shm_zone_t *
ngx_shm_create_zone(ngx_conf_t *cf, ngx_dynamic_healthcheck_conf_t *conf,
    void *tag);

ngx_int_t
ngx_shm_add_to_zone(ngx_dynamic_healthcheck_conf_t *conf, ngx_shm_zone_t *zone);

#define NGX_CONF_UNSET_ZONE ((ngx_shm_zone_t *) NGX_CONF_UNSET_PTR)

#ifdef _WITH_LUA_API

ngx_int_t
lua_get_shm_string(lua_State *L, ngx_str_t *str,
    ngx_slab_pool_t *slab, int index);

#endif

#ifdef __cplusplus
}
#endif


#endif /* NGX_DYNAMIC_SHM_H */

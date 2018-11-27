/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#include "ngx_dynamic_healthcheck_api.h"

#include <assert.h>

#ifdef _WITH_LUA_API
#include "ngx_http_lua_api.h"
#endif

extern ngx_str_t NGX_DH_MODULE_HTTP;


ngx_int_t
ngx_dynamic_healthcheck_api_base::do_disable
    (ngx_dynamic_healthcheck_conf_t *conf, ngx_flag_t disable)
{
    if (conf->shared->disabled == disable)
        return NGX_DECLINED;

    conf->shared->disabled = disable;
    conf->shared->updated = 1;

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0, "[%V] %V %s",
                  &conf->config.module, &conf->config.upstream,
                  disable ? "disable" : "enable");

    return NGX_OK;
}


ngx_int_t
ngx_dynamic_healthcheck_api_base::do_disable_host
    (ngx_dynamic_healthcheck_conf_t *conf, ngx_str_t *host, ngx_flag_t disable)
{
    ngx_uint_t        i;
    ngx_slab_pool_t  *slab = conf->peers.shared->slab;
    ngx_str_array_t  *disabled_hosts;
    ngx_str_array_t   new_disabled_hosts;

    ngx_shmtx_lock(&slab->mutex);

    disabled_hosts = &conf->shared->disabled_hosts;
    
    for (i = 0; i < disabled_hosts->len; i++) {
        if (ngx_memn2cmp(host->data, disabled_hosts->data[i].data,
                         host->len, disabled_hosts->data[i].len) == 0) {
            if (disable) {
                ngx_shmtx_unlock(&slab->mutex);
                return NGX_DECLINED;
            }

            for (i++; i < disabled_hosts->len; i++)
                disabled_hosts->data[i - 1] = disabled_hosts->data[i];

            ngx_str_null(&disabled_hosts->data[disabled_hosts->len - 1]);

            assert(disabled_hosts->len > 0);
            disabled_hosts->len--;

            conf->shared->updated = 1;

            ngx_shmtx_unlock(&slab->mutex);

            ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                          "[%V] %V enable host: %V",
                          &conf->config.module, &conf->config.upstream,
                          host);

            return NGX_OK;
        }
    }

    if (!disable) {
        ngx_shmtx_unlock(&slab->mutex);
        return NGX_DECLINED;
    }

    if (disabled_hosts->len == disabled_hosts->reserved) {
        ngx_memzero(&new_disabled_hosts, sizeof(ngx_str_array_t));
        if (ngx_shm_str_array_create(&new_disabled_hosts,
            ngx_max(2, disabled_hosts->reserved) * 2, slab) == NGX_ERROR) {
            ngx_shmtx_unlock(&slab->mutex);
            return NGX_ERROR;
        }
        ngx_shm_str_array_copy(&new_disabled_hosts, disabled_hosts, slab);
        *disabled_hosts = new_disabled_hosts;
    }

    assert(disabled_hosts->len < disabled_hosts->reserved);

    if (ngx_shm_str_copy(&disabled_hosts->data[disabled_hosts->len], host,
                         slab) == NGX_ERROR) {
        ngx_shmtx_unlock(&slab->mutex);
        return NGX_ERROR;
    }

    disabled_hosts->len++;

    conf->shared->updated = 1;
    
    ngx_shmtx_unlock(&slab->mutex);

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0,
                  "[%V] %V disable host: %V",
                  &conf->config.module, &conf->config.upstream,
                  host);

    return NGX_OK;
}


ngx_int_t
ngx_dynamic_healthcheck_api_base::do_update
    (ngx_dynamic_healthcheck_conf_t *conf,
     ngx_dynamic_healthcheck_opts_t *opts,
     ngx_flag_t flags)
{
    ngx_slab_pool_t                 *slab = conf->peers.shared->slab;
    ngx_dynamic_healthcheck_opts_t   sh;
    ngx_flag_t                       b = 1;

    ngx_memzero(&sh, sizeof(ngx_dynamic_healthcheck_opts_t));

    ngx_shmtx_lock(&slab->mutex);

    if (flags & NGX_DYNAMIC_UPDATE_OPT_TYPE)
        b = b && NGX_OK == ngx_shm_str_copy(&sh.type, &opts->type, slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_URI)
        b = b && NGX_OK == ngx_shm_str_copy(&sh.request_uri, &opts->request_uri,
                                            slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_METHOD)
        b = b && NGX_OK == ngx_shm_str_copy(&sh.request_method,
                                            &opts->request_method, slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_BODY)
        b = b && NGX_OK == ngx_shm_str_copy(&sh.request_body,
                                            &opts->request_body, slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_BODY)
        b = b && NGX_OK == ngx_shm_str_copy(&sh.response_body,
                                            &opts->response_body, slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_CODES)
        b = b && NGX_OK == ngx_shm_num_array_copy(&sh.response_codes,
                                                  &opts->response_codes, slab);
    if (flags & NGX_DYNAMIC_UPDATE_OPT_HEADERS)
        b = b && NGX_OK == ngx_shm_keyval_array_copy(&sh.request_headers,
                                                     &opts->request_headers,
                                                     slab);

    if (!b)
        goto nomem;

    if (flags & NGX_DYNAMIC_UPDATE_OPT_OFF)
        conf->shared->off = opts->off;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_DISABLED)
        conf->shared->disabled = opts->disabled;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_FALL)
        conf->shared->fall = opts->fall;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_RISE)
        conf->shared->rise = opts->rise;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_TIMEOUT)
    conf->shared->timeout = opts->timeout;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_INTERVAL)
        conf->shared->interval = opts->interval;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_KEEPALIVE)
        conf->shared->keepalive = opts->keepalive;

    if (flags & NGX_DYNAMIC_UPDATE_OPT_TYPE)
        conf->shared->type = sh.type;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_URI)
        conf->shared->request_uri = sh.request_uri;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_METHOD)
        conf->shared->request_method = sh.request_method;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_BODY)
        conf->shared->request_body = sh.request_body;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_BODY)
        conf->shared->response_body = sh.response_body;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_RESPONSE_CODES)
        conf->shared->response_codes = sh.response_codes;
    if (flags & NGX_DYNAMIC_UPDATE_OPT_HEADERS)
        conf->shared->request_headers = sh.request_headers;

    conf->shared->updated = 1;

    ngx_shmtx_unlock(&slab->mutex);

    ngx_log_error(NGX_LOG_NOTICE, ngx_cycle->log, 0, "[%V] %V update",
                  &conf->config.module, &conf->config.upstream);

    return NGX_OK;

nomem:

    ngx_shm_str_free(&sh.type, slab);
    ngx_shm_str_free(&sh.request_uri, slab);
    ngx_shm_str_free(&sh.request_method, slab);
    ngx_shm_str_free(&sh.request_body, slab);
    ngx_shm_str_free(&sh.response_body, slab);
    ngx_shm_keyval_array_free(&sh.request_headers, slab);
    ngx_shm_num_array_free(&sh.response_codes, slab);

    ngx_shmtx_unlock(&slab->mutex);

    return NGX_ERROR;
}

#ifdef _WITH_LUA_API

void
ngx_dynamic_healthcheck_api_base::healthcheck_push(lua_State *L,
    ngx_dynamic_healthcheck_conf_t *conf)
{
    ngx_uint_t                      i;
    ngx_dynamic_healthcheck_opts_t *opts;

    if (conf->shared == NULL) {
        lua_pushnil(L);
        return;
    }

    opts = conf->shared;

    if (opts->type.data == NULL) {
        lua_pushnil(L);
        return;
    }

    ngx_shmtx_lock(&conf->peers.shared->slab->mutex);

    lua_newtable(L);

    lua_pushlstring(L, (char *) opts->type.data,
                    opts->type.len);
    lua_setfield(L, -2, "type");

    lua_pushinteger(L, opts->fall);
    lua_setfield(L, -2, "fall");

    lua_pushinteger(L, opts->rise);
    lua_setfield(L, -2, "rise");

    lua_pushinteger(L, opts->timeout);
    lua_setfield(L, -2, "timeout");

    lua_pushinteger(L, opts->interval);
    lua_setfield(L, -2, "interval");

    lua_pushinteger(L, opts->off);
    lua_setfield(L, -2, "off");

    lua_pushinteger(L, opts->disabled);
    lua_setfield(L, -2, "disabled");

    lua_newtable(L);

    for (i = 0; i < opts->disabled_hosts.len; i++) {
        lua_pushlstring(L, (char *) opts->disabled_hosts.data[i].data,
                        opts->disabled_hosts.data[i].len);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "disabled_hosts");

    lua_newtable(L);

    for (i = 0; i < opts->excluded_hosts.len; i++) {
        lua_pushlstring(L, (char *) opts->excluded_hosts.data[i].data,
                        opts->excluded_hosts.data[i].len);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "excluded_hosts");

    lua_pushinteger(L, opts->keepalive);
    lua_setfield(L, -2, "keepalive");

    if (opts->request_uri.len != 0 || opts->request_body.len != 0) {
        lua_newtable(L);

        if (opts->request_uri.len != 0) {
            lua_pushlstring(L, (char *) opts->request_uri.data,
                                        opts->request_uri.len);
            lua_setfield(L, -2, "uri");

            lua_pushlstring(L, (char *) opts->request_method.data,
                                        opts->request_method.len);
            lua_setfield(L, -2, "method");
        }

        if (opts->request_headers.len != 0) {
            lua_newtable(L);

            for (i = 0; i < opts->request_headers.len; ++i) {
                lua_pushlstring(L,
                    (char *) opts->request_headers.data[i].key.data,
                    opts->request_headers.data[i].key.len);
                lua_pushlstring(L,
                    (char *) opts->request_headers.data[i].value.data,
                    opts->request_headers.data[i].value.len);
                lua_rawset(L, -3);
            }

            lua_setfield(L, -2, "headers");
        }

        if (opts->request_body.len != 0) {
            lua_pushlstring(L, (char *) opts->request_body.data,
                                        opts->request_body.len);
            lua_setfield(L, -2, "body");
        }

        if (opts->response_codes.len != 0 || opts->response_body.len != 0)
        {
            lua_newtable(L);

            if (opts->response_codes.len != 0) {
                lua_newtable(L);

                for (i = 0; i < opts->response_codes.len; ++i) {
                    lua_pushinteger(L, opts->response_codes.data[i]);
                    lua_rawseti(L, -2, i + 1);
                }

                lua_setfield(L, -2, "codes");
            }

            if (opts->response_body.len != 0) {
                lua_pushlstring(L, (char *) opts->response_body.data,
                                            opts->response_body.len);
                lua_setfield(L, -2, "body");
            }

            lua_setfield(L, -2, "expected");
        }

        lua_setfield(L, -2, "command");
    }

    ngx_shmtx_unlock(&conf->peers.shared->slab->mutex);
}


static ngx_int_t
get_field_number(lua_State *L, int index, const char *field,
    ngx_flag_t *flags, ngx_flag_t flag)
{
    ngx_int_t  n = 0;
    lua_getfield(L, index, field);
    if (!lua_isnil(L, -1)) {
        n = lua_tointeger(L, -1);
        *flags |= flag;
    }
    lua_pop(L, 1);
    return n;
}


static ngx_str_t
get_field_string(lua_State *L, int index, const char *field,
    ngx_flag_t *flags, ngx_flag_t flag)
{
    ngx_str_t  s = { 0, 0 };
    lua_getfield(L, index, field);
    if (!lua_isnil(L, -1)) {
        s.data = (u_char *) lua_tostring(L, index);
        if (s.data != NULL) {
            s.len = ngx_strlen(s.data);
            *flags |= flag;
        }
    }
    lua_pop(L, 1);
    return s;
}


static ngx_int_t
lua_get_pool_string(lua_State *L, ngx_str_t *str,
    ngx_pool_t *pool, int index)
{
    const char *s = lua_tostring(L, index);
    ngx_str_null(str);
    if (s != NULL) {
        str->len = strlen(s);
        str->data = (u_char *) ngx_pcalloc(pool, str->len + 1);
        if (str->data != NULL)
            ngx_memcpy(str->data, s, str->len);
        else
            str->len = 0;
    }
    return str->data != NULL ? NGX_OK : NGX_ERROR;
}


static ngx_int_t
ngx_pool_keyval_array_create(ngx_keyval_array_t *src, ngx_uint_t size,
    ngx_pool_t *pool)
{
    src->data = (ngx_keyval_t *) ngx_pcalloc(pool, size * sizeof(ngx_keyval_t));
    if (src->data == NULL)
       return NGX_ERROR;

    src->reserved = size;
    return NGX_OK;
}


static ngx_int_t
ngx_pool_num_array_create(ngx_num_array_t *src, ngx_uint_t size,
    ngx_pool_t *pool)
{
    src->data = (ngx_int_t *) ngx_pcalloc(pool, size * sizeof(ngx_int_t));
    if (src->data == NULL)
       return NGX_ERROR;

    src->reserved = size;

    return NGX_OK;
}


#define ngx_http_lua_req_key  "__ngx_req"


static ngx_inline ngx_http_request_t *
ngx_http_lua_get_req(lua_State *L)
{
    ngx_http_request_t    *r;

    lua_getglobal(L, ngx_http_lua_req_key);
    r = (ngx_http_request_t *) lua_touserdata(L, -1);
    lua_pop(L, 1);

    return r;
}


int
ngx_dynamic_healthcheck_api_base::do_update(lua_State *L,
    ngx_dynamic_healthcheck_conf_t *conf)
{
    ngx_dynamic_healthcheck_opts_t  opts;
    ngx_slab_pool_t                *slab = conf->peers.shared->slab;
    ngx_uint_t                      i;
    ngx_flag_t                      is_http;
    int                             top = lua_gettop(L);
    ngx_http_request_t             *r;
    ngx_flag_t                      flags = 0;
    ngx_str_t                       s;

    r = ngx_http_lua_get_req(L);
    if (r == NULL)
        return luaL_error(L, "no request");

    is_http = ngx_strcmp(conf->shared->module.data,
                         NGX_DH_MODULE_HTTP.data) == 0;

    ngx_memzero(&opts, sizeof(ngx_dynamic_healthcheck_opts_t));

    ngx_shmtx_lock(&slab->mutex);

    opts.type      = get_field_string(L, 2, "type",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_TYPE);
    opts.fall      = get_field_number(L, 2, "fall",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_FALL);
    opts.rise      = get_field_number(L, 2, "rise",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_RISE);
    opts.timeout   = get_field_number(L, 2, "timeout",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_TIMEOUT);
    opts.interval  = get_field_number(L, 2, "interval",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_INTERVAL);
    opts.keepalive = get_field_number(L, 2, "keepalive",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_KEEPALIVE);
    opts.off       = get_field_number(L, 2, "off",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_OFF);
    opts.disabled  = get_field_number(L, 2, "disabled",
                                      &flags, NGX_DYNAMIC_UPDATE_OPT_DISABLED);

    opts.fall      = ngx_max(opts.fall, 1);
    opts.rise      = ngx_max(opts.rise, 1);
    opts.timeout   = ngx_max(opts.timeout, 10);
    opts.interval  = ngx_max(opts.interval, 1);
    opts.keepalive = ngx_max(opts.keepalive, 1);

    lua_getfield(L, 2, "command");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        goto done;
    }

    opts.request_uri    = get_field_string(L, -1, "uri",
                                           &flags, NGX_DYNAMIC_UPDATE_OPT_URI);
    opts.request_method = get_field_string(L, -1, "method",
                                         &flags, NGX_DYNAMIC_UPDATE_OPT_METHOD);
    opts.request_body   = get_field_string(L, -1, "body",
                                           &flags, NGX_DYNAMIC_UPDATE_OPT_BODY);

    if (is_http) {
        lua_getfield(L, -1, "headers");

        if (lua_istable(L, -1)) {
            lua_pushvalue(L, -1);
            lua_pushnil(L);

            if (ngx_pool_keyval_array_create(&opts.request_headers,
                                             100, r->pool) == NGX_ERROR)
                goto nomem;

            for (i = 0; lua_next(L, -2) && i < 100; i++) {
                lua_pushvalue(L, -2);

                if (lua_get_pool_string(L,
                        &opts.request_headers.data[i].key,
                        r->pool, -1) == NGX_ERROR)
                    goto nomem;
                if (lua_get_pool_string(L,
                        &opts.request_headers.data[i].value,
                        r->pool, -2) == NGX_ERROR)
                    goto nomem;

                lua_pop(L, 2);
                opts.request_headers.len++;
            }

            lua_pop(L, 1);

            opts.request_headers.reserved =
                ngx_min(opts.request_headers.reserved,
                        opts.request_headers.len * 2);
            flags |= NGX_DYNAMIC_UPDATE_OPT_HEADERS;
        }

        lua_pop(L, 1);  // headers
    }

    lua_getfield(L, -1, "expected");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        goto done;
    }

    opts.response_body = get_field_string(L, -1, "body",
                                  &flags, NGX_DYNAMIC_UPDATE_OPT_RESPONSE_BODY);

    if (is_http) {
        lua_getfield(L, -1, "codes");

        if (lua_istable(L, -1)) {
            lua_pushvalue(L, -1);
            lua_pushnil(L);

            if (ngx_pool_num_array_create(&opts.response_codes, 100,
                                          r->pool) == NGX_ERROR)
                goto nomem;

            for (i = 0; lua_next(L, -2) && i < 100; i++) {
                lua_pushvalue(L, -2);

                if (lua_isnumber(L, -2))
                    opts.response_codes.data[i] = lua_tointeger(L, -2);
                else {
                    s.data = (u_char *) lua_tolstring(L, -2, &s.len);
                    opts.response_codes.data[i] = ngx_atoi(s.data, s.len);
                }

                lua_pop(L, 2);
                opts.response_codes.len++;
            }

            lua_pop(L, 1);

            opts.response_codes.reserved = ngx_min(opts.response_codes.reserved,
                                                   opts.response_codes.len * 2);
            flags |= NGX_DYNAMIC_UPDATE_OPT_RESPONSE_CODES;
        }

        lua_pop(L, 1);  // codes
    }

    lua_pop(L, 1);      // expected
    lua_pop(L, 1);      // command

done:

    ngx_shmtx_unlock(&slab->mutex);

    if (ngx_dynamic_healthcheck_api_base::do_update(conf, &opts, flags)
           == NGX_ERROR)
        return luaL_error(L, "no shared memory");

    lua_pushboolean(L, 1);

    return 1;

nomem:

    ngx_shmtx_unlock(&slab->mutex);

    lua_settop(L, top);

    return luaL_error(L, "no memory");
}


static const char *peers_desc[2] = {
    "primary",
    "backup"
};


template <class S, class PeersT, class PeerT> int
get_status(lua_State *L, ngx_dynamic_healthcheck_conf_t *conf)
{
    S                      *uscf;
    PeersT                 *primary, *peers;
    PeerT                  *peer;
    ngx_uint_t              i;
    ngx_dynamic_hc_stat_t   stat;
    
    uscf = (S *) conf->uscf;

    primary = (PeersT *) uscf->peer.data;
    peers = primary;

    lua_newtable(L);

    ngx_rwlock_rlock(&primary->rwlock);

    for (i = 0; peers && i < 2; peers = peers->next, i++) {
        lua_newtable(L);

        for (peer = peers->peer; peer; peer = peer->next) {
            if (ngx_dynamic_healthcheck_state_stat(&conf->peers,
                    &peer->name, &stat) == NGX_OK) {
                lua_pushlstring(L, (const char *) peer->name.data,
                                peer->name.len);

                lua_newtable(L);

                lua_pushinteger(L, stat.fall);
                lua_setfield(L, -2, "fall");

                lua_pushinteger(L, stat.rise);
                lua_setfield(L, -2, "rise");

                lua_pushinteger(L, stat.fall_total);
                lua_setfield(L, -2, "fall_total");

                lua_pushinteger(L, stat.rise_total);
                lua_setfield(L, -2, "rise_total");

                lua_pushinteger(L, peer->down);
                lua_setfield(L, -2, "down");

                lua_rawset(L, -3);
            }
        }

        lua_setfield(L, -2, peers_desc[i]);
    }

    ngx_rwlock_unlock(&primary->rwlock);

    return 1;
}


int
ngx_dynamic_healthcheck_api_base::do_status(lua_State *L,
    ngx_dynamic_healthcheck_conf_t *conf)
{
    if (ngx_strcmp(conf->config.module.data, NGX_DH_MODULE_HTTP.data) == 0)
        return get_status<ngx_http_upstream_srv_conf_t,
                          ngx_http_upstream_rr_peers_t,
                          ngx_http_upstream_rr_peer_t>(L, conf);

    return get_status<ngx_stream_upstream_srv_conf_t,
                      ngx_stream_upstream_rr_peers_t,
                      ngx_stream_upstream_rr_peer_t>(L, conf);
}

#endif

ngx_http_upstream_main_conf_t *
ngx_dynamic_healthcheck_api_base::get_upstream_conf
    (ngx_http_upstream_main_conf_t *)
{
    return (ngx_http_upstream_main_conf_t *)
        ngx_http_cycle_get_module_main_conf(ngx_cycle,
                                            ngx_http_upstream_module);
}


ngx_stream_upstream_main_conf_t *
ngx_dynamic_healthcheck_api_base::get_upstream_conf
    (ngx_stream_upstream_main_conf_t *)
{
    return (ngx_stream_upstream_main_conf_t *)
        ngx_stream_cycle_get_module_main_conf(ngx_cycle,
                                              ngx_stream_upstream_module);
}


ngx_dynamic_healthcheck_conf_t *
ngx_dynamic_healthcheck_api_base::get_srv_conf(
   ngx_http_upstream_srv_conf_t * uscf)
{
    extern ngx_module_t ngx_http_dynamic_healthcheck_module;
    return (ngx_dynamic_healthcheck_conf_t *)
        ngx_http_conf_upstream_srv_conf(uscf,
            ngx_http_dynamic_healthcheck_module);
}


ngx_dynamic_healthcheck_conf_t *
ngx_dynamic_healthcheck_api_base::get_srv_conf(
    ngx_stream_upstream_srv_conf_t *uscf)
{
    extern ngx_module_t ngx_stream_dynamic_healthcheck_module;
    return (ngx_dynamic_healthcheck_conf_t *)
        ngx_stream_conf_upstream_srv_conf(uscf,
            ngx_stream_dynamic_healthcheck_module);
}



static ngx_int_t
serialize_keyval_array(ngx_keyval_array_t *a, ngx_str_t *s, ngx_pool_t *pool)
{
    ngx_keyval_t *kv = a->data;
    ngx_uint_t    i = 0;
    size_t        sz = a->len * 1024;
    u_char       *last;

    if (a->len == 0) {
        ngx_str_null(s);
        return NGX_OK;
    }

    s->data = (u_char *) ngx_pcalloc(pool, sz);
    if (s->data == NULL)
        return NGX_ERROR;

    last = s->data;

    for (i = 0; i < a->len; i++) {
        last = ngx_snprintf(last, s->data + sz - last, "%V:%V|",
                            &kv[i].key, &kv[i].value);
        if (last == s->data + sz)
            return NGX_ERROR;
    }

    s->len = last - s->data;

    return NGX_OK;
}


static ngx_int_t
serialize_str_array(ngx_str_array_t *a, ngx_str_t *s, ngx_pool_t *pool)
{
    ngx_str_t    *str = a->data;
    ngx_uint_t    i = 0;
    size_t        sz = a->len * 1024;
    u_char       *last;

    if (a->len == 0) {
        ngx_str_null(s);
        return NGX_OK;
    }

    s->data = (u_char *) ngx_pcalloc(pool, sz);
    if (s->data == NULL)
        return NGX_ERROR;

    last = s->data;

    for (i = 0; i < a->len; i++) {
        last = ngx_snprintf(last, s->data + sz - last, "%V|",
                            &str[i]);
        if (last == s->data + sz)
            return NGX_ERROR;
    }

    s->len = last - s->data;

    return NGX_OK;
}

static ngx_int_t
serialize_num_array(ngx_num_array_t *a, ngx_str_t *s, ngx_pool_t *pool)
{
    ngx_int_t    *str = a->data;
    ngx_uint_t    i = 0;
    size_t        sz = a->len * 30;
    u_char       *last;

    if (a->len == 0) {
        ngx_str_null(s);
        return NGX_OK;
    }

    s->data = (u_char *) ngx_pcalloc(pool, sz);
    if (s->data == NULL)
        return NGX_ERROR;

    last = s->data;

    for (i = 0; i < a->len; i++) {
        last = ngx_snprintf(last, s->data + sz - last, "%d|",
                            str[i]);
        if (last == s->data + sz)
            return NGX_ERROR;
    }

    s->len = last - s->data;

    return NGX_OK;
}


static ngx_str_t *
nvl_str(ngx_str_t *s)
{
    static ngx_str_t empty_str = ngx_string("");
    return s->len ? s : &empty_str;
}


static FILE *
healthcheck_open(ngx_dynamic_healthcheck_conf_t *conf,
    const char *mode, ngx_pool_t *pool)
{
    ngx_dynamic_healthcheck_opts_t  *shared = conf->shared;
    ngx_str_t                        dir, path;
    FILE                            *f = NULL;
    ngx_core_conf_t                 *ccf;
    ngx_log_t                       *log = pool->log;

    ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                           ngx_core_module);

    path.data = (u_char *) ngx_pcalloc(pool, 10240);
    dir.data = (u_char *) ngx_pcalloc(pool, 10240);
    if (path.data == NULL || dir.data == NULL)
        goto nomem;

    if (ccf->working_directory.len != 0)
        dir.len = ngx_snprintf(dir.data, 10240, "%V/%V/%V",
            &ccf->working_directory, &conf->config.persistent,
            &shared->module) - dir.data;
    else
        dir.len = ngx_snprintf(dir.data, 10240, "%V/%V",
            &conf->config.persistent, &shared->module) - dir.data;

    if (dir.len == 10240)
        goto nomem;

    path.len = ngx_snprintf(path.data, 10240, "%V/%V",
        &dir, &shared->upstream) - path.data;

    if (path.len == 10240)
        goto nomem;

    if (ngx_create_full_path(path.data, ngx_dir_access(NGX_FILE_OWNER_ACCESS))
            != NGX_OK) {
        ngx_log_error(NGX_LOG_CRIT, log, 0, "can't create directory: %V",
                      &dir);
        return NULL;
    }

    f = fopen((const char *) path.data, mode);
    if (f == NULL)
        ngx_log_error(NGX_LOG_WARN, log, 0, "can't open file: %V",
                      &path);

    return f;

nomem:

    ngx_log_error(NGX_LOG_CRIT, log, 0, "open healthcheck: no memory");

    return NULL;
}

#undef  LF
#define LF "\n"

ngx_int_t
ngx_dynamic_healthcheck_api_base::save(ngx_dynamic_healthcheck_conf_t *conf,
    ngx_log_t *log)
{
    ngx_dynamic_healthcheck_opts_t  *shared = conf->shared;
    ngx_pool_t                      *pool;
    ngx_str_t                        content, headers, codes, hosts;
    FILE                            *f = NULL;

    if (shared->updated == 0)
        return NGX_OK;

    ngx_log_error(NGX_LOG_INFO, log, 0,
                  "[%V] %V: healthcheck save",
                  &shared->module, &shared->upstream);

    pool = ngx_create_pool(1024, log);
    if (pool == NULL)
        goto nomem;

    f = healthcheck_open(conf, "w+", pool);
    if (f == NULL) {
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    content.data = (u_char *) ngx_pcalloc(pool, 10240);
    if (content.data == NULL)
        goto nomem;

    if (serialize_keyval_array(&shared->request_headers,
                               &headers, pool) != NGX_OK)
        goto nomem;

    if (serialize_str_array(&shared->disabled_hosts, &hosts, pool) != NGX_OK)
        goto nomem;

    if (serialize_num_array(&shared->response_codes, &codes, pool) != NGX_OK)
        goto nomem;

    content.len = ngx_snprintf(content.data, 10240, "type:%V"              LF
                                                    "fall:%d"              LF
                                                    "rise:%d"              LF
                                                    "timeout:%d"           LF
                                                    "interval:%d"          LF
                                                    "keepalive:%d"         LF
                                                    "request_body:\"%V\""  LF
                                                    "response_body:\"%V\"" LF
                                                    "off:%d"               LF
                                                    "disabled:%d"          LF
                                                    "disabled_hosts:%V"    LF,
                               &shared->type,
                               shared->fall,
                               shared->rise,
                               shared->timeout,
                               shared->interval,
                               shared->keepalive,
                               nvl_str(&shared->request_body),
                               nvl_str(&shared->response_body),
                               shared->off,
                               shared->disabled,
                               &hosts) - content.data;

    if (content.len == 10240)
        goto nomem;

    if (ngx_strncmp(shared->module.data, "http", 3) == 0) {
        content.len = ngx_snprintf(content.data + content.len,
                                   10240 - content.len, "request_uri:%V"     LF
                                                        "request_method:%V"  LF
                                                        "request_headers:%V" LF
                                                        "response_codes:%V"  LF,
                                   nvl_str(&shared->request_uri),
                                   nvl_str(&shared->request_method),
                                   &headers,
                                   &codes) - content.data;
        if (content.len == 10240)
            goto nomem;
    }

    if (fwrite(content.data, content.len, 1, f) == 0)
        ngx_log_error(NGX_LOG_CRIT, log, errno, "healthcheck: failed to save");

    fclose(f);

    ngx_destroy_pool(pool);

    shared->updated = 0;
    shared->loaded = (ngx_timeofday())->sec;

    return NGX_OK;

nomem:

    if (f != NULL)
        fclose(f);

    if (pool != NULL)
        ngx_destroy_pool(pool);

    ngx_log_error(NGX_LOG_CRIT, log, 0, "save healthcheck: no memory");

    return NGX_ERROR;
}


static ngx_str_t *
temp_str(u_char *data, size_t len, ngx_str_t *temp)
{
    temp->data = data;
    temp->len = len;
    return temp;
}

ngx_int_t
ngx_dynamic_healthcheck_api_base::load(ngx_dynamic_healthcheck_conf_t *conf,
    ngx_log_t *log)
{
    ngx_dynamic_healthcheck_opts_t  *shared = conf->shared;
    ngx_pool_t                      *pool;
    ngx_str_t                        content;
    FILE                            *f = NULL;
    struct stat                      attr;
    ngx_int_t                        rc;

    pool = ngx_create_pool(1024, log);
    if (pool == NULL)
        goto nomem;

    f = healthcheck_open(conf, "r", pool);
    if (f == NULL) {
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    if (-1 == fstat(fileno(f), &attr)) {
        ngx_log_error(NGX_LOG_ERR, log, errno,
                      "load healthcheck: can't get fstat");
        fclose(f);
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    if (attr.st_mtime <= shared->loaded) {
        fclose(f);
        ngx_destroy_pool(pool);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_INFO, log, 0,
                  "[%V] %V: healthcheck reload (%d:%d)",
                  &shared->module, &shared->upstream,
                  attr.st_mtime, shared->loaded);

    content.len = attr.st_size;
    content.data = (u_char *) ngx_pcalloc(pool, content.len + 1);
    if (content.data == NULL)
        goto nomem;

    if (fread(content.data, content.len, 1, f) != 1) {
        ngx_log_error(NGX_LOG_CRIT, log, errno, "healthcheck: failed to read");
        fclose(f);
        ngx_destroy_pool(pool);
        return NGX_ERROR;
    }

    fclose(f);

    rc = ngx_dynamic_healthcheck_api_base::parse(conf, &content, pool);

    ngx_destroy_pool(pool);

    if (rc == NGX_OK) {
        shared->loaded = attr.st_mtime;
        return NGX_OK;
    }

    return NGX_ERROR;

nomem:

    if (f != NULL)
        fclose(f);

    if (pool != NULL)
        ngx_destroy_pool(pool);

    ngx_log_error(NGX_LOG_CRIT, log, 0, "load healthcheck: no memory");

    return NGX_ERROR;
}


ngx_int_t
ngx_dynamic_healthcheck_api_base::parse(ngx_dynamic_healthcheck_conf_t *conf,
    ngx_str_t *content, ngx_pool_t *pool)
{
    ngx_dynamic_healthcheck_opts_t  *shared = conf->shared;
    ngx_log_t                       *log = pool->log;
    ngx_str_array_t                  hosts;
    ngx_num_array_t                  codes;
    ngx_keyval_array_t               headers;
    ngx_regex_compile_t              rc;
    u_char                           errstr[NGX_MAX_CONF_ERRSTR];
    int                              m;
    int                             *capt;
    ngx_str_t                        temp;
    ngx_slab_pool_t                 *slab;
    const char                      *sep;

    static ngx_str_t re_http =
        ngx_string("type:([^\n]+)"                LF
                   "fall:(\\d+)"                  LF
                   "rise:(\\d+)"                  LF
                   "timeout:(\\d+)"               LF
                   "interval:(\\d+)"              LF
                   "keepalive:(\\d+)"             LF
                   "request_body:\"([^\"]*)\""    LF
                   "response_body:\"([^\"]*)\""   LF
                   "off:(\\d+)"                   LF
                   "disabled:(\\d+)"              LF
                   "disabled_hosts:([^\n]*)"      LF
                   "request_uri:([^\n]*)"         LF
                   "request_method:([^\n]*)"      LF
                   "request_headers:([^\n]*)"     LF
                   "response_codes:([^\n]*)"      LF);
    static ngx_str_t re_tcp =
        ngx_string("type:([^\n]+)"                LF
                   "fall:(\\d+)"                  LF
                   "rise:(\\d+)"                  LF
                   "timeout:(\\d+)"               LF
                   "interval:(\\d+)"              LF
                   "keepalive:(\\d+)"             LF
                   "request_body:\"([^\"]*)\""    LF
                   "response_body:\"([^\"]*)\""   LF
                   "off:(\\d+)"                   LF
                   "disabled:(\\d+)"              LF
                   "disabled_hosts:([^\n]*)"      LF);

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;
    rc.pool = pool;
    rc.options = PCRE_UNGREEDY;

    if (ngx_strncmp(shared->module.data, "http", 3) == 0)
        rc.pattern = re_http;
    else
        rc.pattern = re_tcp;

    if (ngx_regex_compile(&rc) != NGX_OK) {
        ngx_log_error(NGX_LOG_CRIT, log, 0, "healthcheck: %V", &rc.err);
        return NGX_ERROR;
    }

    capt = (int *) ngx_pcalloc(pool, sizeof(int) * ((1 + rc.captures) * 3));
    if (capt == NULL)
        goto nomem;

    m = ngx_regex_exec(rc.regex, content, capt, (1 + rc.captures) * 3);

    if (m == NGX_REGEX_NO_MATCHED) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "healthcheck: failed to parse: \n%V",
                      &content);
        return NGX_ERROR;
    }

    slab = (ngx_slab_pool_t *) conf->zone->shm.addr;

    if (ngx_shm_str_copy(&shared->type,
                         temp_str(content->data + capt[2],
                                  capt[3] - capt[2], &temp),
                         slab) != NGX_OK)
        goto nomem;

    shared->fall = ngx_atoi(content->data + capt[4], capt[5] - capt[4]);
    shared->rise = ngx_atoi(content->data + capt[6], capt[7] - capt[6]);
    shared->timeout = ngx_atoi(content->data + capt[8], capt[9] - capt[8]);
    shared->interval = ngx_atoi(content->data + capt[10], capt[11] - capt[10]);
    shared->keepalive = ngx_atoi(content->data + capt[12], capt[13] - capt[12]);

    if (ngx_shm_str_copy(&shared->request_body,
                         temp_str(content->data + capt[14],
                                  capt[15] - capt[14], &temp),
                         slab) != NGX_OK)
        goto nomem;

    if (ngx_shm_str_copy(&shared->response_body,
                         temp_str(content->data + capt[16],
                                  capt[17] - capt[16], &temp),
                         slab) != NGX_OK)
        goto nomem;

    shared->off = ngx_atoi(content->data + capt[18], capt[19] - capt[18]);
    shared->disabled = ngx_atoi(content->data + capt[20], capt[21] - capt[20]);

    // disabled_hosts;
    hosts.data = (ngx_str_t *) ngx_pcalloc(pool, 100 * sizeof(ngx_str_t));
    if (hosts.data == NULL)
        goto nomem;
    hosts.reserved = 100;
    hosts.len = 0;

    temp_str(content->data + capt[22], capt[23] - capt[22], &temp);
    temp.data[temp.len] = 0;

    for (sep = ngx_strchr(temp.data, '|');
         sep && hosts.len < 100;
         sep = ngx_strchr(temp.data, '|')) {
        ngx_str_t host;
        host.data = temp.data;
        host.len = (u_char *) sep - temp.data;
        host.data[host.len] = 0;
        hosts.data[hosts.len++] = host;
        temp.data = (u_char *) sep + 1;
    }

    hosts.reserved = ngx_min(hosts.len * 2, hosts.reserved);
    if (ngx_shm_str_array_copy(&shared->disabled_hosts, &hosts,
                               slab) != NGX_OK)
        goto nomem;

    if (ngx_strncmp(shared->module.data, "http", 3) == 0) {
        if (ngx_shm_str_copy(&shared->request_uri,
                             temp_str(content->data + capt[24],
                                      capt[25] - capt[24], &temp),
                             slab) != NGX_OK)
            goto nomem;

        if (ngx_shm_str_copy(&shared->request_method,
                             temp_str(content->data + capt[26],
                                      capt[27] - capt[26], &temp),
                             slab) != NGX_OK)
            goto nomem;

        // request_headers

        headers.data = (ngx_keyval_t *) ngx_pcalloc(pool,
            100 * sizeof(ngx_keyval_t));
        if (headers.data == NULL)
            goto nomem;
        headers.reserved = 100;
        headers.len = 0;

        temp_str(content->data + capt[28], capt[29] - capt[28], &temp);
        temp.data[temp.len] = 0;

        for (sep = ngx_strchr(temp.data, '|');
             sep && headers.len < 100;
             sep = ngx_strchr(temp.data, '|')) {
            ngx_keyval_t kv;
            kv.key.data = temp.data;
            kv.key.len = (u_char *) ngx_strchr(kv.key.data, ':') - kv.key.data;
            kv.key.data[kv.key.len] = 0;
            kv.value.data = kv.key.data + kv.key.len + 1;
            kv.value.len = (u_char *) sep - kv.value.data;
            kv.value.data[kv.value.len] = 0;
            headers.data[headers.len++] = kv;
            temp.data = (u_char *) sep + 1;
        }

        headers.reserved = ngx_min(headers.len * 2, headers.reserved);
        if (ngx_shm_keyval_array_copy(&shared->request_headers, &headers,
                                      slab) != NGX_OK)
            goto nomem;

        // response_codes

        codes.data = (ngx_int_t *) ngx_pcalloc(pool,
            100 * sizeof(ngx_int_t));
        if (codes.data == NULL)
            goto nomem;
        codes.reserved = 100;
        codes.len = 0;

        temp_str(content->data + capt[30], capt[31] - capt[30], &temp);
        temp.data[temp.len] = 0;

        for (sep = ngx_strchr(temp.data, '|');
             sep && codes.len < 100;
             sep = ngx_strchr(temp.data, '|')) {
            codes.data[codes.len++] = ngx_atoi(temp.data,
                                               (u_char *) sep - temp.data);
            temp.data = (u_char *) sep + 1;
        }

        codes.reserved = ngx_min(codes.len * 2, codes.reserved);
        if (ngx_shm_num_array_copy(&shared->response_codes, &codes,
                                   slab) != NGX_OK)
            goto nomem;
    }

    return NGX_OK;

nomem:

    ngx_log_error(NGX_LOG_CRIT, log, 0, "parse healthcheck: no memory");

    return NGX_ERROR;
}

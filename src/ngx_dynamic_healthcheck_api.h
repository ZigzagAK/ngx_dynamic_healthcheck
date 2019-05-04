/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_API_H
#define NGX_DYNAMIC_HEALTHCHECK_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_stream.h>

    
#ifdef _WITH_LUA_API
#include <lauxlib.h>
#endif

#ifdef __cplusplus
}
#endif

#include "ngx_dynamic_healthcheck.h"
#include "ngx_dynamic_shm.h"


ngx_inline ngx_flag_t str_eq(ngx_str_t s1, ngx_str_t s2)
{
    return ngx_memn2cmp(s1.data, s2.data, s1.len, s2.len) == 0;
}


class ngx_dynamic_healthcheck_api_base {
    static ngx_int_t
    parse(ngx_dynamic_healthcheck_conf_t *conf,
          ngx_str_t *content, ngx_pool_t *pool);

public:

    static ngx_http_upstream_main_conf_t *
    get_upstream_conf(ngx_http_upstream_main_conf_t *);

    static ngx_stream_upstream_main_conf_t *
    get_upstream_conf(ngx_stream_upstream_main_conf_t *);

    static ngx_dynamic_healthcheck_conf_t *
    get_srv_conf(ngx_http_upstream_srv_conf_t *uscf);

    static ngx_dynamic_healthcheck_conf_t *
    get_srv_conf(ngx_stream_upstream_srv_conf_t *uscf);

    static ngx_int_t
    save(ngx_dynamic_healthcheck_conf_t *conf, ngx_log_t *log);

    static ngx_int_t
    load(ngx_dynamic_healthcheck_conf_t *conf, ngx_log_t *log);

protected:

    static ngx_int_t
    do_update(ngx_dynamic_healthcheck_conf_t *conf,
              ngx_dynamic_healthcheck_opts_t *opts,
              ngx_flag_t flags);

    static ngx_int_t
    do_disable(ngx_dynamic_healthcheck_conf_t *conf, ngx_flag_t disable);

    static ngx_int_t
    do_disable_host(ngx_dynamic_healthcheck_conf_t *conf, ngx_str_t *host,
                    ngx_flag_t disable);

    static void
    do_disable_host(ngx_http_upstream_srv_conf_t *uscf, ngx_str_t *host,
                    ngx_flag_t disable);

    static void
    do_disable_host(ngx_stream_upstream_srv_conf_t *uscf, ngx_str_t *host,
                    ngx_flag_t disable);

#ifdef _WITH_LUA_API

    static int
    healthcheck_push(lua_State *L, ngx_dynamic_healthcheck_conf_t *conf);

    static int
    do_lua_update(lua_State *L, ngx_dynamic_healthcheck_conf_t *conf);

    static int
    do_lua_status(lua_State *L, ngx_dynamic_healthcheck_conf_t *conf);

#endif
};


template <class M, class S> class ngx_dynamic_healthcheck_api :
    private ngx_dynamic_healthcheck_api_base {

#ifdef _WITH_LUA_API

public:

    typedef int (fun_t)(lua_State *L, ngx_dynamic_healthcheck_conf_t *uscf);

#endif

private:

#ifdef _WITH_LUA_API

    static int
    lua_error(lua_State *L, const char *err)
    {
        lua_pushnil(L);
        lua_pushlstring(L, err, strlen(err));
        return 2;
    }

    static int
    do_lua_fun(fun_t f, lua_State *L, S *uscf)
    {
        ngx_dynamic_healthcheck_conf_t *conf;

        if (uscf->shm_zone == NULL)
            return lua_error(L, "only for upstream with 'zone'");

        conf = get_srv_conf(uscf);
        if (conf == NULL)
            return lua_error(L, "can't get healthcheck module configuration");

        return f(L, conf);
    }

#endif

    static ngx_dynamic_healthcheck_conf_t *
    healthcheck_conf(S *uscf)
    {
        if (uscf->shm_zone == NULL)
            return NULL;

        return get_srv_conf(uscf);
    }

    static ngx_int_t
    do_update(S *uscf, ngx_dynamic_healthcheck_opts_t *opts, ngx_flag_t flags)
    {
        ngx_dynamic_healthcheck_conf_t *conf = healthcheck_conf(uscf);

        if (conf == NULL)
            return NGX_ERROR;

        return ngx_dynamic_healthcheck_api_base::do_update(conf, opts, flags);
    }

    static ngx_int_t
    do_disable(S *uscf, ngx_flag_t disable)
    {
        ngx_dynamic_healthcheck_conf_t *conf = healthcheck_conf(uscf);

        if (conf == NULL)
            return NGX_ERROR;

        return ngx_dynamic_healthcheck_api_base::do_disable(conf, disable);
    }

    static ngx_int_t
    do_disable_host(S *uscf, ngx_str_t *host, ngx_flag_t disable)
    {
        ngx_dynamic_healthcheck_conf_t *conf = healthcheck_conf(uscf);

        if (conf == NULL)
            return NGX_ERROR;

        if (ngx_peer_excluded(host, conf))
            ngx_dynamic_healthcheck_api_base::do_disable_host(uscf, host,
                                                              disable);

        return ngx_dynamic_healthcheck_api_base::do_disable_host(conf, host,
                                                                 disable);
    }

public:

    static ngx_int_t
    update(ngx_dynamic_healthcheck_opts_t *opts, ngx_flag_t flags)
    {
        ngx_uint_t    i;
        M            *umcf = NULL;
        S           **uscf;

        if (opts->upstream.len == 0)
            return NGX_ERROR;

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return NGX_ERROR;
        uscf = (S **) umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++)
            if (str_eq(opts->upstream, uscf[i]->host))
                return ngx_dynamic_healthcheck_api<M, S>::do_update(uscf[i],
                                                                    opts,
                                                                    flags);

        return NGX_DECLINED;
    }

    static ngx_int_t
    disable(ngx_str_t upstream, ngx_flag_t disable)
    {
        ngx_uint_t    i;
        M            *umcf = NULL;
        S           **uscf;
        ngx_int_t     rc;

        if (upstream.len == 0)
            return NGX_ERROR;

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return NGX_ERROR;
        uscf = (S **) umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            if (!str_eq(upstream, uscf[i]->host))
                continue;

            rc = ngx_dynamic_healthcheck_api<M, S>::do_disable
                (uscf[i], disable);

            if (rc == NGX_OK)
                ngx_dynamic_healthcheck_api<M, S>::refresh_timers();

            return rc;
        }

        return NGX_DECLINED;
    }

    static ngx_int_t
    disable_host(ngx_str_t upstream, ngx_str_t *host, ngx_flag_t disable)
    {
        ngx_uint_t    i;
        M            *umcf = NULL;
        S           **uscf;
        ngx_uint_t    updated = 0;

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return NGX_ERROR;
        uscf = (S **) umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            if (upstream.len != 0 && !str_eq(upstream, uscf[i]->host))
                continue;

            switch (ngx_dynamic_healthcheck_api<M, S>::do_disable_host
                        (uscf[i], host, disable))
            {
                case NGX_OK:
                    updated++;
                    break;

                case NGX_ERROR:
                default:
                    if (upstream.len != 0)
                        return NGX_ERROR;
            }

            if (upstream.len != 0)
                break;
        }

        if (updated == 0)
            return NGX_DECLINED;

        ngx_dynamic_healthcheck_api<M, S>::refresh_timers();

        return NGX_OK;
    }

#ifdef _WITH_LUA_API

    static int
    lua_get(lua_State *L)
    {
        ngx_uint_t     i;
        M             *umcf = NULL;
        S            **uscf;
        ngx_str_t      upstream;

        ngx_str_null(&upstream);

        if (lua_gettop(L) == 1 && !lua_isnil(L, 1)) {
            upstream.data = (u_char *) luaL_checkstring(L, 1);
            upstream.len = ngx_strlen(upstream.data);
            if (upstream.len == 0)
                return luaL_error(L, "non empty upstream required");
        } else if (lua_gettop(L) > 1)
            return lua_error(L, "1 or 0 arguments expected");

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return lua_error(L, "not initialized");
        uscf = (S **) umcf->upstreams.elts;

        if (upstream.len == 0)
            lua_newtable(L);

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            if (uscf[i]->srv_conf == NULL)
                continue;

            if (upstream.len == 0 && uscf[i]->shm_zone == NULL)
                continue;

            if (upstream.len == 0 || str_eq(upstream, uscf[i]->host)) {

                if (upstream.len == 0)
                    lua_pushlstring(L, (char *) uscf[i]->host.data,
                                    uscf[i]->host.len);

                ngx_dynamic_healthcheck_api<M, S>::do_lua_fun
                    (&ngx_dynamic_healthcheck_api_base::healthcheck_push,
                     L, uscf[i]);

                if (upstream.len == 0) {

                    lua_rawset(L, -3);
                    continue;
                }

                if (!lua_isnil(L, -1))
                    return 1;

                lua_pushliteral(L, "no healthcheck");
                return 2;
            }
        }

        if (upstream.len == 0)
            return 1;

        return lua_error(L, "upstream not found");
    }

    static int
    lua_update(lua_State *L)
    {
        ngx_uint_t    i;
        M            *umcf = NULL;
        S           **uscf;
        ngx_str_t     upstream;

        if (lua_gettop(L) != 2)
            return lua_error(L, "2 arguments expected");

        upstream.data = (u_char *) luaL_checkstring(L, 1);
        upstream.len = ngx_strlen(upstream.data);

        if (upstream.len == 0)
            return luaL_error(L, "non empty upstream required");

        if (!lua_istable(L, 2))
            return luaL_error(L, "table expected on 2nd argument");

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return lua_error(L, "not initialized");
        uscf = (S **) umcf->upstreams.elts;

        for (i = 0; i < umcf->upstreams.nelts; i++)
            if (str_eq(upstream, uscf[i]->host))
                return ngx_dynamic_healthcheck_api<M, S>::do_lua_fun
                    (&ngx_dynamic_healthcheck_api_base::do_lua_update,
                     L, uscf[i]);

        return lua_error(L, "upstream not found");
    }

    static int
    lua_disable_host(lua_State *L)
    {
        ngx_str_t     upstream;
        ngx_str_t     host;
        ngx_flag_t    disabled;
        int           n = lua_gettop(L);

        if (n != 2 && n != 3)
            return lua_error(L, "2 or 3 arguments expected");

        ngx_str_null(&upstream);
        ngx_str_null(&host);

        host.data = (u_char *) luaL_checkstring(L, 1);
        host.len = ngx_strlen(host.data);

        if (host.len == 0)
            return luaL_error(L, "non empty host required");

        disabled = lua_toboolean(L, 2);

        if (n == 3 && !lua_isnil(L, 3)) {
            upstream.data = (u_char *) luaL_checkstring(L, 3);
            upstream.len = ngx_strlen(upstream.data);
            if (upstream.len == 0)
                return luaL_error(L, "non empty upstream required");
        }

        switch (ngx_dynamic_healthcheck_api<M, S>::disable_host(upstream,
                                                                &host,
                                                                disabled))
        {
            case NGX_OK:
                lua_pushboolean(L, 1);
                return 1;

            case NGX_DECLINED:
                return lua_error(L, "upstream not found");

        }

        return lua_error(L, "only for upstream with 'zone'");
    }

    static int
    lua_disable(lua_State *L)
    {
        ngx_str_t     upstream;
        ngx_flag_t    disabled;
        int           n = lua_gettop(L);

        if (n != 2)
            return lua_error(L, "2 arguments expected");

        ngx_str_null(&upstream);

        upstream.data = (u_char *) luaL_checkstring(L, 1);
        upstream.len = ngx_strlen(upstream.data);

        if (upstream.len == 0)
            return luaL_error(L, "non empty upstream required");

        disabled = lua_toboolean(L, 2);

        switch (ngx_dynamic_healthcheck_api<M, S>::disable(upstream, disabled))
        {
            case NGX_OK:
                lua_pushboolean(L, 1);
                return 1;

            case NGX_DECLINED:
                return lua_error(L, "upstream not found");

        }

        return lua_error(L, "only for upstream with 'zone'");
    }

    static int
    lua_status(lua_State *L)
    {
        ngx_uint_t     i;
        M             *umcf = NULL;
        S            **uscf;
        ngx_str_t      upstream;

        ngx_str_null(&upstream);

        if (lua_gettop(L) == 1 && !lua_isnil(L, 1)) {
            upstream.data = (u_char *) luaL_checkstring(L, 1);
            upstream.len = ngx_strlen(upstream.data);
            if (upstream.len == 0)
                return luaL_error(L, "non empty upstream required");
        } else if (lua_gettop(L) > 1)
            return lua_error(L, "1 or 0 arguments expected");

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return lua_error(L, "not initialized");
        uscf = (S **) umcf->upstreams.elts;

        if (upstream.len == 0)
            lua_newtable(L);

        for (i = 0; i < umcf->upstreams.nelts; i++) {

            if (uscf[i]->srv_conf == NULL)
                continue;

            if (upstream.len == 0 && uscf[i]->shm_zone == NULL)
                continue;

            if (upstream.len == 0 || str_eq(upstream, uscf[i]->host)) {

                if (upstream.len == 0)
                    lua_pushlstring(L, (char *) uscf[i]->host.data,
                                    uscf[i]->host.len);

                ngx_dynamic_healthcheck_api<M, S>::do_lua_fun
                    (&ngx_dynamic_healthcheck_api_base::do_lua_status,
                     L, uscf[i]);

                if (upstream.len == 0) {

                    lua_rawset(L, -3);
                    continue;
                }

                if (!lua_isnil(L, -1))
                    return 1;

                lua_pushliteral(L, "no healthcheck");
                return 2;
            }
        }

        if (upstream.len == 0)
            return 1;

        return lua_error(L, "upstream not found");
    }

#endif
    
    static void
    refresh_timers(ngx_log_t *log = ngx_cycle->log)
    {
        ngx_uint_t                        i;
        M                                *umcf = NULL;
        S                               **uscf;
        ngx_dynamic_healthcheck_conf_t   *conf;
        ngx_dynamic_healthcheck_event_t  *event;
        ngx_msec_t                        now;
        ngx_time_t                       *tp;
        ngx_core_conf_t                  *ccf;
        ngx_flag_t                        persistent;

        ccf = (ngx_core_conf_t *) ngx_get_conf(ngx_cycle->conf_ctx,
                                               ngx_core_module);

        umcf = get_upstream_conf(umcf);
        if (umcf == NULL)
            return;

        uscf = (S **) umcf->upstreams.elts;

        ngx_time_update();

        tp = ngx_timeofday();

        now = tp->sec * 1000 + tp->msec;
        
        for (i = 0; i < umcf->upstreams.nelts; i++) {

            if (ngx_process == NGX_PROCESS_WORKER
                && i % ccf->worker_processes != ngx_worker)
                continue;

            if (uscf[i]->shm_zone == NULL)
                continue;

            conf = get_srv_conf(uscf[i]);

            if (conf == NULL)
                continue;

            if (conf->shared == NULL)
                continue;

            ngx_shmtx_lock(&conf->shared->state.slab->mutex);

            if (conf->shared->type.len == 0)
                goto next;

            if (conf->event.data != NULL) {
                conf->shared->last = now;
                goto next;
            }

            if (!conf->shared->updated
                && conf->shared->last + 5000 > now)
                goto next;

            persistent = conf->config.persistent.len != 0 &&
                ngx_strcmp(conf->config.persistent.data, "off") != 0;
            if (persistent)
                load(conf, log);

            if (conf->shared->off || conf->shared->interval == 0) {
                ngx_log_debug2(NGX_LOG_DEBUG_HTTP, log, 0,
                               "[%V] %V healthcheck off",
                               &conf->shared->module,
                               &conf->shared->upstream);
                goto next;
            }

            ngx_memzero(&conf->event, sizeof(ngx_event_t));

            event = (ngx_dynamic_healthcheck_event_t *)
                ngx_calloc(sizeof(ngx_dynamic_healthcheck_event_t), log);
            if (event == NULL) {
                ngx_shmtx_unlock(&conf->shared->state.slab->mutex);
                ngx_log_error(NGX_LOG_ERR, log, 0, "healthcheck: no memory");
                return;
            }

            event->dumb_conn.fd = -1;
            event->uscf = (void *) uscf[i];
            event->conf = conf;
            event->completed = &ngx_dynamic_healthcheck_api<M, S>::on_completed;
            event->updated = conf->shared->updated;

            conf->event.log = log;
            conf->event.data = (void *) event;
            conf->event.handler = &ngx_dynamic_event_handler<S>::check;

            conf->shared->last = now;

            ngx_add_timer(&conf->event, 0);

next:

            ngx_shmtx_unlock(&conf->shared->state.slab->mutex);
        }
    }

private:

    static void
    on_completed(ngx_dynamic_healthcheck_event_t *event)
    {
        ngx_shmtx_lock(&event->conf->shared->state.slab->mutex);

        if (event->conf->config.persistent.len != 0
            && ngx_strcmp(event->conf->config.persistent.data, "off") != 0)
            ngx_dynamic_healthcheck_api_base::save(event->conf, event->log);
        else if (event->updated == event->conf->shared->updated)
            event->conf->shared->updated = 0;

        ngx_shmtx_unlock(&event->conf->shared->state.slab->mutex);
    }
};


ngx_inline ngx_int_t
ngx_dynamic_healthcheck_update(ngx_dynamic_healthcheck_opts_t *opts,
    ngx_flag_t flags)
{
    extern ngx_str_t NGX_DH_MODULE_HTTP;

    if (opts->module.len == 0)
        return NGX_AGAIN;

    if (opts->upstream.len == 0)
        return NGX_AGAIN;
    
    if (opts->module.data == NGX_DH_MODULE_HTTP.data)
        return ngx_dynamic_healthcheck_api<ngx_http_upstream_main_conf_t,
            ngx_http_upstream_srv_conf_t>::update(opts, flags);

    return ngx_dynamic_healthcheck_api<ngx_stream_upstream_main_conf_t,
               ngx_stream_upstream_srv_conf_t>::update(opts, flags);
}


ngx_inline ngx_int_t
ngx_dynamic_healthcheck_disable(ngx_str_t module, ngx_str_t upstream,
    ngx_flag_t disable)
{
    extern ngx_str_t NGX_DH_MODULE_HTTP;

    if (module.len == 0)
        return NGX_AGAIN;

    if (upstream.len == 0)
        return NGX_AGAIN;

    if (module.data == NGX_DH_MODULE_HTTP.data)
        return ngx_dynamic_healthcheck_api<ngx_http_upstream_main_conf_t,
            ngx_http_upstream_srv_conf_t>::disable(upstream, disable);

    return ngx_dynamic_healthcheck_api<ngx_stream_upstream_main_conf_t,
               ngx_stream_upstream_srv_conf_t>::disable(upstream, disable);
}


ngx_inline ngx_int_t
ngx_dynamic_healthcheck_disable_host(ngx_str_t module, ngx_str_t upstream,
    ngx_str_t host, ngx_flag_t disable)
{
    extern ngx_str_t NGX_DH_MODULE_HTTP;

    if (module.len == 0)
        return NGX_AGAIN;

    if (module.data == NGX_DH_MODULE_HTTP.data)
        return ngx_dynamic_healthcheck_api<ngx_http_upstream_main_conf_t,
            ngx_http_upstream_srv_conf_t>::disable_host(upstream, &host,
                                                        disable);

    return ngx_dynamic_healthcheck_api<ngx_stream_upstream_main_conf_t,
               ngx_stream_upstream_srv_conf_t>::disable_host(upstream, &host,
                                                             disable);
}

#endif /* NGX_DYNAMIC_HEALTHCHECK_API_H */

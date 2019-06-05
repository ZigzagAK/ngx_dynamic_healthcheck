/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

extern "C" {
#include <ngx_stream.h>
}

#ifdef _WITH_LUA_API
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include "ngx_stream_lua_request.h"
#include "ngx_stream_lua_api.h"
}
#endif

#include "ngx_dynamic_healthcheck.h"
#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_healthcheck_config.h"
#include "ngx_dynamic_healthcheck_api.h"
#include "ngx_dynamic_healthcheck_state.h"


static ngx_command_t ngx_stream_dynamic_healthcheck_commands[] = {

    { ngx_string("healthcheck"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_ANY,
      ngx_dynamic_healthcheck_check,
      NGX_STREAM_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("healthcheck_buffer_size"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, buffer_size),
      NULL },

    { ngx_string("healthcheck_request_body"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, request_body),
      NULL },

    { ngx_string("healthcheck_response_body"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, response_body),
      NULL },

    { ngx_string("healthcheck_disable_host"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, disabled_hosts_global),
      NULL },

    { ngx_string("healthcheck_persistent"),
      NGX_STREAM_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, persistent),
      NULL },

    { ngx_string("check"),
      NGX_STREAM_UPS_CONF|NGX_CONF_ANY,
      ngx_dynamic_healthcheck_check,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_uri"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE2,
      ngx_http_dynamic_healthcheck_check_request_uri,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_headers"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_request_headers,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_response_codes"),
      NGX_STREAM_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_response_codes,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, request_body),
      NULL },

    { ngx_string("check_response_body"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, response_body),
      NULL },

    { ngx_string("check_disable_host"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, disabled_hosts),
      NULL },

    { ngx_string("check_persistent"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, persistent),
      NULL },

    { ngx_string("check_exclude_host"),
      NGX_STREAM_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_STREAM_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, excluded_hosts),
      NULL },

    ngx_null_command

};


#ifdef _WITH_LUA_API
static ngx_int_t
ngx_stream_dynamic_healthcheck_post_conf(ngx_conf_t *cf);
#else
#define ngx_stream_dynamic_healthcheck_post_conf NULL
#endif


static char *
ngx_stream_dynamic_healthcheck_init_main_conf(ngx_conf_t *cf, void *conf);


static void *
ngx_stream_dynamic_healthcheck_create_conf(ngx_conf_t *cf);


static ngx_stream_module_t ngx_stream_dynamic_healthcheck_ctx = {
    NULL,                                            /* preconfiguration  */
    ngx_stream_dynamic_healthcheck_post_conf,        /* postconfiguration */
    ngx_stream_dynamic_healthcheck_create_conf,      /* create main       */
    ngx_stream_dynamic_healthcheck_init_main_conf,   /* init main         */
    ngx_stream_dynamic_healthcheck_create_conf,      /* create server     */
    NULL                                             /* merge server      */
};


ngx_module_t ngx_stream_dynamic_healthcheck_module = {
    NGX_MODULE_V1,
    &ngx_stream_dynamic_healthcheck_ctx,         /* module context    */
    ngx_stream_dynamic_healthcheck_commands,     /* module directives */
    NGX_STREAM_MODULE,                           /* module type       */
    NULL,                                        /* init master       */
    NULL,                                        /* init module       */
    NULL,                                        /* init process      */
    NULL,                                        /* init thread       */
    NULL,                                        /* exit thread       */
    NULL,                                        /* exit process      */
    NULL,                                        /* exit master       */
    NGX_MODULE_V1_PADDING
};


ngx_str_t NGX_DH_MODULE_STREAM = ngx_string("stream");


// initialization

#ifdef _WITH_LUA_API

int
ngx_stream_dynamic_healthcheck_create_module(lua_State *L)
{
    lua_createtable(L, 0, 5);

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_stream_upstream_main_conf_t,
         ngx_stream_upstream_srv_conf_t>::lua_get, 0);
    lua_setfield(L, -2, "get");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_stream_upstream_main_conf_t,
         ngx_stream_upstream_srv_conf_t>::lua_update, 0);
    lua_setfield(L, -2, "update");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_stream_upstream_main_conf_t,
         ngx_stream_upstream_srv_conf_t>::lua_disable_host, 0);
    lua_setfield(L, -2, "disable_host");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_stream_upstream_main_conf_t,
         ngx_stream_upstream_srv_conf_t>::lua_disable, 0);
    lua_setfield(L, -2, "disable");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_stream_upstream_main_conf_t,
         ngx_stream_upstream_srv_conf_t>::lua_status, 0);
    lua_setfield(L, -2, "status");

    return 1;
}


static ngx_int_t
ngx_stream_dynamic_healthcheck_post_conf(ngx_conf_t *cf)
{
    if (ngx_stream_lua_add_package_preload(cf, "ngx.healthcheck",
        ngx_stream_dynamic_healthcheck_create_module) != NGX_OK)
        return NGX_ERROR;

    return NGX_OK;
}

#endif

static void *
ngx_stream_dynamic_healthcheck_create_conf(ngx_conf_t *cf)
{
    ngx_dynamic_healthcheck_conf_t *conf;

    conf = (ngx_dynamic_healthcheck_conf_t *) ngx_pcalloc(cf->pool,
        sizeof(ngx_dynamic_healthcheck_conf_t));
    if (conf == NULL)
        return NULL;

    conf->config.disabled_hosts_global.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;
    conf->config.disabled_hosts.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;
    conf->config.excluded_hosts.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;
    conf->config.response_codes.data = (ngx_int_t *) NGX_CONF_UNSET_PTR;
    conf->config.request_headers.data = (ngx_keyval_t *) NGX_CONF_UNSET_PTR;

    conf->config.module      = NGX_DH_MODULE_STREAM;
    conf->config.fall        = NGX_CONF_UNSET;
    conf->config.rise        = NGX_CONF_UNSET;
    conf->config.timeout     = NGX_CONF_UNSET_UINT;
    conf->config.interval    = NGX_CONF_UNSET;
    conf->config.keepalive   = NGX_CONF_UNSET_UINT;
    conf->config.buffer_size = NGX_CONF_UNSET_SIZE;

    return conf;
}


static void
ngx_stream_dynamic_healthcheck_init_peers(ngx_dynamic_healthcheck_conf_t *conf)
{
    ngx_stream_upstream_srv_conf_t  *uscf;
    ngx_stream_upstream_rr_peers_t  *primary, *peers;
    ngx_stream_upstream_rr_peer_t   *peer;
    ngx_uint_t                       i;
    ngx_dynamic_hc_stat_t            stat;

    uscf = (ngx_stream_upstream_srv_conf_t *) conf->uscf;

    primary = (ngx_stream_upstream_rr_peers_t *) uscf->peer.data;
    peers = primary;

    ngx_rwlock_rlock(&primary->rwlock);

    for (i = 0; peers && i < 2; peers = peers->next, i++)
        for (peer = peers->peer; peer; peer = peer->next) {
            if (ngx_peer_excluded(&peer->name, conf)
                || ngx_peer_excluded(&peer->server, conf))
                continue;
            if (ngx_peer_disabled(&peer->name, conf)
                || ngx_peer_disabled(&peer->server, conf)) {
                peer->down = 1;
                continue;
            }
            if (ngx_dynamic_healthcheck_state_stat(&conf->peers,
                    &peer->server, &peer->name, &stat) == NGX_OK) {
                peer->down = stat.down;
            }
        }

    ngx_rwlock_unlock(&primary->rwlock);
}


static ngx_int_t
ngx_stream_dynamic_healthcheck_init_srv_conf(ngx_conf_t *cf,
    ngx_stream_upstream_srv_conf_t *uscf)
{
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_dynamic_healthcheck_conf_t *main_conf;

    if (uscf->srv_conf == NULL)
        return NGX_OK;

    main_conf = (ngx_dynamic_healthcheck_conf_t *)
        ngx_stream_conf_get_module_main_conf(cf,
            ngx_stream_dynamic_healthcheck_module);

    if (main_conf->config.buffer_size == NGX_CONF_UNSET_SIZE)
        main_conf->config.buffer_size = ngx_pagesize;

    conf = (ngx_dynamic_healthcheck_conf_t *)
        ngx_stream_conf_upstream_srv_conf(uscf,
            ngx_stream_dynamic_healthcheck_module);

    if (conf->config.type.len != 0 && uscf->shm_zone == NULL) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0,
            "'check' directive requires "
            "'zone' directive in upstream %V in %s:%ud",
            &uscf->host, uscf->file_name, uscf->line);
        return NGX_ERROR;
    }

    ngx_conf_merge_str_value(conf->config.type, main_conf->config.type);
    ngx_conf_merge_value(conf->config.fall,
        main_conf->config.fall, 1);
    ngx_conf_merge_value(conf->config.rise,
        main_conf->config.rise, 1);
    ngx_conf_merge_value(conf->config.timeout,
        main_conf->config.timeout, 1000);
    ngx_conf_merge_value(conf->config.interval,
        main_conf->config.interval, 10);
    ngx_conf_merge_value(conf->config.passive,
        main_conf->config.passive, 0);
    ngx_conf_merge_str_value(conf->config.request_body,
        main_conf->config.request_body);
    ngx_conf_merge_str_value(conf->config.response_body,
        main_conf->config.response_body);
    ngx_conf_merge_value(conf->config.off, main_conf->config.off, 0);
    ngx_conf_merge_value(conf->config.disabled, main_conf->config.disabled, 0);
    ngx_conf_merge_array_value(conf->config.disabled_hosts,
        main_conf->config.disabled_hosts);
    ngx_conf_merge_str_value(conf->config.persistent,
        main_conf->config.persistent);

    if (conf->config.type.data != NULL
        && ngx_strncmp(conf->config.type.data, "http", 4) == 0)
        if (conf->config.request_uri.len == 0) {
            ngx_str_null(&conf->config.request_method);
            ngx_memzero(&conf->config.request_headers,
                        sizeof(ngx_keyval_array_t));
            ngx_str_null(&conf->config.request_body);
            ngx_str_null(&conf->config.response_body);

            conf->config.keepalive = 1;
            ngx_memzero(&conf->config.response_codes, sizeof(ngx_num_array_t));
        }

    conf->config.buffer_size = main_conf->config.buffer_size;
    conf->config.disabled_hosts_global =
            main_conf->config.disabled_hosts_global;
    conf->config.upstream = uscf->host;

    if (conf->config.buffer_size < conf->config.request_body.len) {
        ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
            "'healthcheck_buffer_size' is lesser than 'request_body'");
        return NGX_ERROR;
    }

    conf->uscf = uscf;
    conf->post_init = ngx_stream_dynamic_healthcheck_init_peers;
    conf->zone = ngx_shm_create_zone(cf, conf,
        &ngx_stream_dynamic_healthcheck_module);

    if (conf->zone == NULL)
        return NGX_ERROR;

    return NGX_OK;
}


static char *
ngx_stream_dynamic_healthcheck_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_stream_upstream_srv_conf_t     **b, **e;
    ngx_stream_upstream_main_conf_t     *umcf;

    umcf = (ngx_stream_upstream_main_conf_t *)
        ngx_stream_conf_get_module_main_conf(cf, ngx_stream_upstream_module);

    b = (ngx_stream_upstream_srv_conf_t **) umcf->upstreams.elts;
    e = b + umcf->upstreams.nelts;

    for (; b < e; ++b)
        if (ngx_stream_dynamic_healthcheck_init_srv_conf(cf, *b) != NGX_OK)
            return (char *) NGX_CONF_ERROR;

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
                  "stream dynamic healthcheck module loaded");

    return NGX_CONF_OK;
}

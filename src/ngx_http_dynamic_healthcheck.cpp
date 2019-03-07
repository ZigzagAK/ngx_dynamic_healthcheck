/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifdef _WITH_LUA_API
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include "ngx_http_lua_api.h"
}
#endif

#include "ngx_dynamic_healthcheck.h"
#include "ngx_dynamic_shm.h"
#include "ngx_dynamic_healthcheck_config.h"
#include "ngx_dynamic_healthcheck_api.h"
#include "ngx_dynamic_healthcheck_state.h"


static char *
ngx_http_dynamic_healthcheck_get(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


static char *
ngx_http_dynamic_healthcheck_update(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);


static char *
ngx_http_dynamic_healthcheck_status(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);



static ngx_command_t ngx_http_dynamic_healthcheck_commands[] = {

    { ngx_string("healthcheck"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_ANY,
      ngx_dynamic_healthcheck_check,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("healthcheck_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_ANY,
      ngx_conf_set_size_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, buffer_size),
      NULL },

    { ngx_string("healthcheck_request_uri"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
      ngx_http_dynamic_healthcheck_check_request_uri,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("healthcheck_request_headers"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_request_headers,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("healthcheck_request_body"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, request_body),
      NULL },

    { ngx_string("healthcheck_response_codes"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_response_codes,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("healthcheck_response_body"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, response_body),
      NULL },

    { ngx_string("healthcheck_disable_host"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, disabled_hosts_global),
      NULL },

    { ngx_string("healthcheck_persistent"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, persistent),
      NULL },

    { ngx_string("check"),
      NGX_HTTP_UPS_CONF|NGX_CONF_ANY,
      ngx_dynamic_healthcheck_check,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_uri"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE2,
      ngx_http_dynamic_healthcheck_check_request_uri,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_headers"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_request_headers,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_request_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, request_body),
      NULL },

    { ngx_string("check_disable_host"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, disabled_hosts),
      NULL },

    { ngx_string("check_response_codes"),
      NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
      ngx_http_dynamic_healthcheck_check_response_codes,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("check_response_body"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, response_body),
      NULL },

    { ngx_string("check_exclude_host"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot2,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, excluded_hosts),
      NULL },

    { ngx_string("check_persistent"),
      NGX_HTTP_UPS_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_dynamic_healthcheck_opts_t, persistent),
      NULL },

    { ngx_string("healthcheck_get"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_dynamic_healthcheck_get,
      0,
      0,
      NULL },

    { ngx_string("healthcheck_update"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_dynamic_healthcheck_update,
      0,
      0,
      NULL },

    { ngx_string("healthcheck_status"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_dynamic_healthcheck_status,
      0,
      0,
      NULL },

    ngx_null_command

};


static ngx_int_t
ngx_http_dynamic_healthcheck_post_conf(ngx_conf_t *cf);


static char *
ngx_http_dynamic_healthcheck_init_main_conf(ngx_conf_t *cf, void *conf);


static void *
ngx_http_dynamic_healthcheck_create_conf(ngx_conf_t *cf);


static ngx_http_module_t ngx_http_dynamic_healthcheck_ctx = {
    NULL,                                          /* preconfiguration  */
    ngx_http_dynamic_healthcheck_post_conf,        /* postconfiguration */
    ngx_http_dynamic_healthcheck_create_conf,      /* create main       */
    ngx_http_dynamic_healthcheck_init_main_conf,   /* init main         */
    ngx_http_dynamic_healthcheck_create_conf,      /* create server     */
    NULL,                                          /* merge server      */
    NULL,                                          /* create location   */
    NULL                                           /* merge location    */
};


ngx_module_t ngx_http_dynamic_healthcheck_module = {
    NGX_MODULE_V1,
    &ngx_http_dynamic_healthcheck_ctx,         /* module context    */
    ngx_http_dynamic_healthcheck_commands,     /* module directives */
    NGX_HTTP_MODULE,                           /* module type       */
    NULL,                                      /* init master       */
    NULL,                                      /* init module       */
    ngx_dynamic_healthcheck_init_worker,       /* init process      */
    NULL,                                      /* init thread       */
    NULL,                                      /* exit thread       */
    NULL,                                      /* exit process      */
    NULL,                                      /* exit master       */
    NGX_MODULE_V1_PADDING
};


ngx_str_t NGX_DH_MODULE_HTTP = ngx_string("http");


// initialization

#ifdef _WITH_LUA_API

static int
ngx_http_dynamic_healthcheck_create_module(lua_State *L)
{
    lua_createtable(L, 0, 5);

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_http_upstream_main_conf_t,
         ngx_http_upstream_srv_conf_t>::lua_get, 0);
    lua_setfield(L, -2, "get");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_http_upstream_main_conf_t,
         ngx_http_upstream_srv_conf_t>::lua_update, 0);
    lua_setfield(L, -2, "update");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_http_upstream_main_conf_t,
         ngx_http_upstream_srv_conf_t>::lua_disable_host, 0);
    lua_setfield(L, -2, "disable_host");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_http_upstream_main_conf_t,
         ngx_http_upstream_srv_conf_t>::lua_disable, 0);
    lua_setfield(L, -2, "disable");

    lua_pushcclosure(L, &ngx_dynamic_healthcheck_api
        <ngx_http_upstream_main_conf_t,
         ngx_http_upstream_srv_conf_t>::lua_status, 0);
    lua_setfield(L, -2, "status");

    return 1;
}


extern int
ngx_stream_dynamic_healthcheck_create_module(lua_State *L);

#endif


static ngx_int_t
ngx_http_dynamic_healthcheck_touch(ngx_http_request_t *r)
{
    ngx_dynamic_healthcheck_conf_t  *uscf;
    ngx_http_upstream_state_t       *state;

    if (r->upstream == NULL
        || r->upstream->upstream == NULL
        || r->upstream->upstream->srv_conf == NULL)
        return NGX_OK;

    uscf = (ngx_dynamic_healthcheck_conf_t *)
        ngx_http_conf_upstream_srv_conf(r->upstream->upstream,
            ngx_http_dynamic_healthcheck_module);

    if (uscf == NULL || uscf->shared == NULL)
        return NGX_OK;

    if (!uscf->shared->passive)
        return NGX_OK;

    state = r->upstream->state;

    if (state == NULL || state->peer == NULL)
        return NGX_OK;

    if (state->status < NGX_HTTP_SPECIAL_RESPONSE)
        ngx_dynamic_healthcheck_state_checked(&uscf->peers, state->peer);

    return NGX_OK;
}


static ngx_int_t
ngx_http_dynamic_healthcheck_post_conf(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t   *mcf;
    ngx_http_handler_pt         *handler;

#ifdef _WITH_LUA_API

    if (ngx_http_lua_add_package_preload(cf, "ngx.healthcheck",
        ngx_http_dynamic_healthcheck_create_module) != NGX_OK)
        return NGX_ERROR;

    if (ngx_http_lua_add_package_preload(cf, "ngx.healthcheck.stream",
        ngx_stream_dynamic_healthcheck_create_module) != NGX_OK)
        return NGX_ERROR;

#endif

    mcf = (ngx_http_core_main_conf_t *)
        ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    handler = (ngx_http_handler_pt *)
        ngx_array_push(&mcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (handler == NULL)
        return NGX_ERROR;

    *handler = ngx_http_dynamic_healthcheck_touch;

    return NGX_OK;
}


static void *
ngx_http_dynamic_healthcheck_create_conf(ngx_conf_t *cf)
{
    ngx_dynamic_healthcheck_conf_t *conf;

    conf = (ngx_dynamic_healthcheck_conf_t *) ngx_pcalloc(cf->pool,
        sizeof(ngx_dynamic_healthcheck_conf_t));
    if (conf == NULL)
        return NULL;

    conf->config.disabled_hosts_global.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;
    conf->config.disabled_hosts.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;
    conf->config.excluded_hosts.data = (ngx_str_t *) NGX_CONF_UNSET_PTR;

    conf->config.module      = NGX_DH_MODULE_HTTP;
    conf->config.fall        = 1;
    conf->config.rise        = 1;
    conf->config.timeout     = 1000;
    conf->config.interval    = 10;
    conf->config.keepalive   = 1;
    conf->config.buffer_size = NGX_CONF_UNSET_SIZE;

    return conf;
}


static void
ngx_http_dynamic_healthcheck_init_peers(ngx_dynamic_healthcheck_conf_t *conf)
{
    ngx_http_upstream_srv_conf_t  *uscf;
    ngx_http_upstream_rr_peers_t  *primary, *peers;
    ngx_http_upstream_rr_peer_t   *peer;
    ngx_uint_t                     i;
    ngx_dynamic_hc_stat_t          stat;

    uscf = (ngx_http_upstream_srv_conf_t *) conf->uscf;

    primary = (ngx_http_upstream_rr_peers_t *) uscf->peer.data;
    peers = primary;

    ngx_rwlock_rlock(&primary->rwlock);

    for (i = 0; peers && i < 2; peers = peers->next, i++)
        for (peer = peers->peer; peer; peer = peer->next) {
            if (ngx_peer_excluded(&peer->name, conf)
                || ngx_peer_excluded(&peer->server, conf))
                continue;
            if (ngx_dynamic_healthcheck_state_stat(&conf->peers,
                    &peer->server, &peer->name, &stat) == NGX_OK) {
                peer->down = stat.down;
            }
        }

    ngx_rwlock_unlock(&primary->rwlock);
}


static ngx_int_t
ngx_http_dynamic_healthcheck_init_srv_conf(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *uscf)
{
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_dynamic_healthcheck_conf_t *main_conf;

    if (uscf->srv_conf == NULL)
        return NGX_OK;

    main_conf = (ngx_dynamic_healthcheck_conf_t *)
        ngx_http_conf_get_module_main_conf(cf,
            ngx_http_dynamic_healthcheck_module);

    if (main_conf->config.buffer_size == NGX_CONF_UNSET_SIZE)
        main_conf->config.buffer_size = ngx_pagesize - sizeof(ngx_pool_t) - 1;
    
    conf = (ngx_dynamic_healthcheck_conf_t *)
        ngx_http_conf_upstream_srv_conf(uscf,
            ngx_http_dynamic_healthcheck_module);

    ngx_conf_merge_str_value(conf->config.type, main_conf->config.type);
    ngx_conf_merge_uint_value(conf->config.keepalive,
        main_conf->config.keepalive, 1);
    ngx_conf_merge_value(conf->config.passive,
        main_conf->config.passive, 0);
    ngx_conf_merge_str_value(conf->config.request_uri,
        main_conf->config.request_uri);
    ngx_conf_merge_str_value(conf->config.request_method,
        main_conf->config.request_method);
    ngx_conf_merge_str_value(conf->config.request_body,
        main_conf->config.request_body);
    ngx_conf_merge_array_value(conf->config.request_headers,
        main_conf->config.request_headers);
    ngx_conf_merge_str_value(conf->config.response_body,
        main_conf->config.response_body);
    ngx_conf_merge_array_value(conf->config.response_codes,
        main_conf->config.response_codes);
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
    conf->post_init = ngx_http_dynamic_healthcheck_init_peers;
    conf->zone = ngx_shm_create_zone(cf, conf,
        &ngx_http_dynamic_healthcheck_module);

    if (conf->zone == NULL)
        return NGX_ERROR;

    return NGX_OK;
}


static char *
ngx_http_dynamic_healthcheck_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_upstream_srv_conf_t     **b, **e;
    ngx_http_upstream_main_conf_t     *umcf;

    umcf = (ngx_http_upstream_main_conf_t *)
        ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

    b = (ngx_http_upstream_srv_conf_t **) umcf->upstreams.elts;
    e = b + umcf->upstreams.nelts;

    for (; b < e; ++b)
        if (ngx_http_dynamic_healthcheck_init_srv_conf(cf, *b) != NGX_OK)
            return (char *) NGX_CONF_ERROR;

    ngx_log_error(NGX_LOG_NOTICE, cf->log, 0,
                  "http dynamic healthcheck module loaded");

    return NGX_CONF_OK;
}


static ngx_str_t nomem = ngx_string("no memory");


static ngx_str_t *
escape_str(ngx_pool_t *pool, ngx_str_t *s)
{
    ngx_str_t       *tmp;
    u_char          *b, *d;

    tmp = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t));
    if (tmp == NULL)
        return &nomem;

    tmp->data = (u_char *) ngx_pcalloc(pool, s->len * 2);
    if (tmp->data == NULL)
        return &nomem;

    for (b = s->data, d = tmp->data; b < s->data + s->len; b++) {
        if (*b == CR) {
            *d++='\\';
            *d++='r';
        }
        else if (*b == LF) {
            *d++='\\';
            *d++='n';
        }
        else
            *d++=*b;
    }

    tmp->len = d - tmp->data;

    return tmp;
}


static ngx_str_t *
serialize_num_array(ngx_pool_t *pool, ngx_num_array_t *a)
{
    ngx_buf_t  *tmp;
    ngx_uint_t  i;
    ngx_str_t  *s;

    tmp = ngx_create_temp_buf(pool, a->len * 22);
    if (tmp == NULL)
        return &nomem;

    s = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t));
    if (s == NULL)
        return &nomem;

    for (i = 0; i < a->len; i++) {
      tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last, "%d",
                               a->data[i]);
      if (i != a->len - 1)
          tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last, ",");
    }

    s->data = tmp->start;
    s->len = tmp->last - tmp->start;
    
    return s;
}


static ngx_str_t *
serialize_str_array(ngx_pool_t *pool, ngx_str_array_t *a)
{
    ngx_buf_t  *tmp;
    ngx_uint_t  i;
    ngx_str_t  *s;
    size_t      size = 0;

    for (i = 0; i < a->len; i++)
        size += (3 + a->data[i].len);

    tmp = ngx_create_temp_buf(pool, size);
    if (tmp == NULL)
        return &nomem;

    s = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t));
    if (s == NULL)
        return &nomem;

    for (i = 0; i < a->len; i++) {
      tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last, "\"%V\"",
                               &a->data[i]);
      if (i != a->len - 1)
          tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last, ",");
    }

    s->data = tmp->start;
    s->len = tmp->last - tmp->start;

    return s;
}


static ngx_str_t *
serialize_keyval_array(ngx_pool_t *pool, ngx_keyval_array_t *a)
{
    ngx_buf_t  *tmp;
    ngx_uint_t  i;
    ngx_str_t  *s;
    size_t      size = 0;

    for (i = 0; i < a->len; i++)
        size += (10 + a->data[i].key.len + a->data[i].value.len);

    tmp = ngx_create_temp_buf(pool, size);
    if (tmp == NULL)
        return &nomem;

    s = (ngx_str_t *) ngx_pcalloc(pool, sizeof(ngx_str_t));
    if (s == NULL)
        return &nomem;

    for (i = 0; i < a->len; i++) {
      tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last,
                               "\"%V\":\"%V\"",
                               &a->data[i].key, &a->data[i].value);
      if (i != a->len - 1)
          tmp->last = ngx_snprintf(tmp->last, tmp->end - tmp->last, ",");
    }

    s->data = tmp->start;
    s->len = tmp->last - tmp->start;

    return s;
}


static ngx_chain_t *
ngx_http_dynamic_healthcheck_get_hc(ngx_http_request_t *r,
    ngx_dynamic_healthcheck_opts_t *shared, ngx_str_t tab)
{
    ngx_flag_t   is_http = ngx_strncmp(shared->type.data, "http", 4) == 0;
    ngx_chain_t *out = (ngx_chain_t *) ngx_pcalloc(r->pool,
                                                   sizeof(ngx_chain_t));
    if (out == NULL)
        return NULL;
    
    out->buf = ngx_create_temp_buf(r->pool, ngx_pagesize);
    if (out->buf == NULL)
        return NULL;

    if (shared != NULL) {
        ngx_shmtx_lock(&shared->state.slab->mutex);

        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
            "{"                                   CRLF
            "%V    \"rise\":%d,"                  CRLF
            "%V    \"fall\":%d,"                  CRLF
            "%V    \"interval\":%d,"              CRLF
            "%V    \"keepalive\":%d,"             CRLF
            "%V    \"timeout\":%d,"               CRLF
            "%V    \"type\":\"%V\","              CRLF
            "%V    \"port\":%d,"                  CRLF
            "%V    \"passive\":%d,"               CRLF
            "%V    \"command\":{"                 CRLF,
                &tab, shared->rise,
                &tab, shared->fall,
                &tab, shared->interval,
                &tab, shared->keepalive,
                &tab, shared->timeout,
                &tab, &shared->type,
                &tab, shared->port,
                &tab, shared->passive,
                &tab);

        if (is_http) {
            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last,
            "%V        \"uri\":\"%V\","           CRLF
            "%V        \"method\":\"%V\","        CRLF
            "%V        \"headers\":{%V},"         CRLF,
                &tab, &shared->request_uri,
                &tab, &shared->request_method,
                &tab, serialize_keyval_array(r->pool, &shared->request_headers));
        }

        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
            "%V        \"body\":\"%V\","          CRLF
            "%V        \"expected\":{"            CRLF
            "%V            \"body\":\"%V\"",
                &tab, escape_str(r->pool, &shared->request_body),
                &tab,
                &tab, escape_str(r->pool, &shared->response_body));

        if (is_http) {
            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last,
            ","                                 CRLF
            "%V            \"codes\":[%V]"        CRLF,
                &tab, serialize_num_array(r->pool, &shared->response_codes));
        } else
            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last, CRLF);

        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
            "%V        }"                         CRLF
            "%V    },"                            CRLF
            "%V    \"disabled\":%d,"              CRLF
            "%V    \"off\":%d,"                   CRLF
            "%V    \"disabled_hosts\":[%V],"      CRLF
            "%V    \"excluded_hosts\":[%V]"       CRLF
            "%V}",
                &tab,
                &tab,
                &tab, shared->disabled,
                &tab, shared->off,
                &tab, serialize_str_array(r->pool, &shared->disabled_hosts),
                &tab, serialize_str_array(r->pool, &shared->excluded_hosts),
                &tab);

        ngx_shmtx_unlock(&shared->state.slab->mutex);
    }

    return out;
}


static ngx_str_t with_tab = ngx_string("    ");
static ngx_str_t no_tab   = ngx_string("");


template <class M, class S> ngx_chain_t *
ngx_http_dynamic_healthcheck_get(ngx_http_request_t *r,
    ngx_http_variable_value_t *upstream)
{
    S                               **uscf;
    M                                *umcf = NULL;
    ngx_dynamic_healthcheck_conf_t   *conf;
    ngx_chain_t                      *start = NULL, *out = NULL, *next = NULL;
    ngx_uint_t                        i;
    ngx_str_t                         tab = no_tab;

    start = out = (ngx_chain_t *) ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
    if (start == NULL)
        return NULL;
    
    start->buf = ngx_create_temp_buf(r->pool, ngx_pagesize);
    if (start->buf == NULL)
        return NULL;

    umcf = ngx_dynamic_healthcheck_api_base::get_upstream_conf(umcf);

    if (umcf == NULL || umcf->upstreams.nelts == 0)
        goto skip;

    uscf = (S **) umcf->upstreams.elts;

    if (upstream->not_found) {
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "{"CRLF);
        tab = with_tab;
    }
 
    for (i = 0; i < umcf->upstreams.nelts; i++) {

        conf = ngx_dynamic_healthcheck_api_base::get_srv_conf(uscf[i]);
        if (conf == NULL)
            continue;

        if (conf->shared->type.len == 0)
            continue;

        if (upstream->not_found
            || ngx_memn2cmp(upstream->data, conf->shared->upstream.data,
                            upstream->len, conf->shared->upstream.len) == 0) {

            if (upstream->not_found)
                out->buf->last = ngx_snprintf(out->buf->last,
                                              out->buf->end - out->buf->last,
                                              "    \"%V\":",
                                              &conf->shared->upstream);

            next = ngx_http_dynamic_healthcheck_get_hc(r, conf->shared, tab);
            if (next == NULL)
                return NULL;

            if (upstream->not_found && i != umcf->upstreams.nelts - 1)
                next->buf->last = ngx_snprintf(next->buf->last,
                                               next->buf->end - next->buf->last,
                                               ",");
            next->buf->last = ngx_snprintf(next->buf->last,
                                           next->buf->end - next->buf->last,
                                           CRLF);

            out->next = next;
            out = next;
        }
    }

    if (upstream->not_found)
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last, "}"CRLF);

skip:

    if (umcf == NULL || umcf->upstreams.nelts == 0)
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "{}"CRLF);

    out->buf->last_buf = (r == r->main) ? 1: 0;
    out->buf->last_in_chain = 1;

    if (!upstream->not_found && start->next == NULL) {
        // upstream not found
        start->buf->last = start->buf->start;
        return start;
    }

    return upstream->not_found ? start : start->next;
}


static ngx_http_variable_value_t *
get_arg(ngx_http_request_t *r, const char *arg)
{
    ngx_str_t var = { ngx_strlen(arg), (u_char *) arg };
    return ngx_http_get_variable(r, &var, ngx_hash_key(var.data, var.len));
}


static ngx_int_t
ngx_http_dynamic_healthcheck_get_handler(ngx_http_request_t *r)
{
    static ngx_str_t            json = ngx_string("application/json");
    ngx_chain_t                *out, *tmp;
    ngx_int_t                   rc;
    ngx_http_variable_value_t  *upstream;
    ngx_http_variable_value_t  *stream;
    off_t                       content_length = 0;

    if (r->method != NGX_HTTP_GET)
        return NGX_HTTP_NOT_ALLOWED;

    if ((rc = ngx_http_discard_request_body(r)) != NGX_OK)
        return rc;

    upstream = get_arg(r, "arg_upstream");
    stream = get_arg(r, "arg_stream");

    out = stream->not_found
        ? ngx_http_dynamic_healthcheck_get
            <ngx_http_upstream_main_conf_t,
             ngx_http_upstream_srv_conf_t>(r, upstream)
        : ngx_http_dynamic_healthcheck_get
            <ngx_stream_upstream_main_conf_t,
             ngx_stream_upstream_srv_conf_t>(r, upstream);

    if (out == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    for (tmp = out; tmp; tmp = tmp->next)
        content_length += (tmp->buf->last - tmp->buf->start);

    if (content_length != 0) {
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_type = json;
        r->headers_out.content_length_n = content_length;
    } else {
        r->headers_out.status = NGX_HTTP_NOT_FOUND;
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "not found");
        r->headers_out.content_length_n = 9;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK)
        return rc;

    return ngx_http_output_filter(r, out);
}


static void
set_str_opt(ngx_http_variable_value_t *v, ngx_str_t *opt,
    ngx_flag_t *flags, ngx_flag_t flag)
{
    if (v->not_found)
        return;

    opt->data = v->data;
    opt->len = v->len;

    *flags |= flag;
}


template <class N> void
set_num_opt(ngx_http_variable_value_t *v, N *n,
    ngx_flag_t *flags, ngx_flag_t flag)
{
    if (v->not_found)
        return;

    *n = ngx_atoi(v->data, v->len);

    *flags |= flag;
}


ngx_int_t
ngx_http_dynamic_healthcheck_update(ngx_http_request_t *r)
{
    ngx_dynamic_healthcheck_opts_t  opts;
    ngx_flag_t                      flags = 0;
    ngx_int_t                       rc = NGX_OK;

    ngx_http_variable_value_t      *stream;
    ngx_http_variable_value_t      *upstream;
    ngx_http_variable_value_t      *type;
    ngx_http_variable_value_t      *fall;
    ngx_http_variable_value_t      *rise;
    ngx_http_variable_value_t      *timeout;
    ngx_http_variable_value_t      *interval;
    ngx_http_variable_value_t      *keepalive;
    ngx_http_variable_value_t      *request_uri;
    ngx_http_variable_value_t      *request_method;
    ngx_http_variable_value_t      *request_headers;
    ngx_http_variable_value_t      *request_body;
    ngx_http_variable_value_t      *response_codes;
    ngx_http_variable_value_t      *response_body;
    ngx_http_variable_value_t      *off;
    ngx_http_variable_value_t      *disable_host;
    ngx_http_variable_value_t      *enable_host;
    ngx_http_variable_value_t      *disable;
    ngx_http_variable_value_t      *port;
    ngx_http_variable_value_t      *passive;
    u_char                         *c, *s;

    extern ngx_str_t NGX_DH_MODULE_STREAM;

    stream          = get_arg(r, "arg_stream");
    upstream        = get_arg(r, "arg_upstream");
    type            = get_arg(r, "arg_type");
    fall            = get_arg(r, "arg_fall");
    rise            = get_arg(r, "arg_rise");
    timeout         = get_arg(r, "arg_timeout");
    interval        = get_arg(r, "arg_interval");
    keepalive       = get_arg(r, "arg_keepalive");
    port            = get_arg(r, "arg_port");
    passive         = get_arg(r, "arg_passive");
    request_uri     = get_arg(r, "arg_request_uri");
    request_method  = get_arg(r, "arg_request_method");
    request_headers = get_arg(r, "arg_request_headers");
    request_body    = get_arg(r, "arg_request_body");
    response_codes  = get_arg(r, "arg_response_codes");
    response_body   = get_arg(r, "arg_response_body");
    off             = get_arg(r, "arg_off");
    disable_host    = get_arg(r, "arg_disable_host");
    enable_host     = get_arg(r, "arg_enable_host");
    disable         = get_arg(r, "arg_disable");

    ngx_memzero(&opts, sizeof(ngx_dynamic_healthcheck_opts_t));

    opts.module = stream->not_found ? NGX_DH_MODULE_HTTP : NGX_DH_MODULE_STREAM;

    if (!upstream->not_found) {
        opts.upstream.data = upstream->data;
        opts.upstream.len = upstream->len;
    }

    set_str_opt(type, &opts.type, &flags, NGX_DYNAMIC_UPDATE_OPT_TYPE);
    set_num_opt<ngx_int_t>(fall, &opts.fall,
                           &flags, NGX_DYNAMIC_UPDATE_OPT_FALL);
    set_num_opt<ngx_int_t>(rise, &opts.rise,
                           &flags, NGX_DYNAMIC_UPDATE_OPT_RISE);
    set_num_opt<ngx_int_t>(timeout, &opts.timeout,
                           &flags, NGX_DYNAMIC_UPDATE_OPT_TIMEOUT);
    set_num_opt<ngx_int_t>(interval, &opts.interval,
                           &flags, NGX_DYNAMIC_UPDATE_OPT_INTERVAL);
    set_num_opt<ngx_uint_t>(keepalive, &opts.keepalive,
                            &flags, NGX_DYNAMIC_UPDATE_OPT_KEEPALIVE);
    set_num_opt<ngx_uint_t>(port, &opts.port,
                            &flags, NGX_DYNAMIC_UPDATE_OPT_PORT);
    set_num_opt<ngx_flag_t>(passive, &opts.passive,
                            &flags, NGX_DYNAMIC_UPDATE_OPT_PASSIVE);
    set_str_opt(request_uri, &opts.request_uri,
                &flags, NGX_DYNAMIC_UPDATE_OPT_URI);
    set_str_opt(request_method, &opts.request_method,
                &flags, NGX_DYNAMIC_UPDATE_OPT_METHOD);
    set_str_opt(request_body, &opts.request_body,
                &flags, NGX_DYNAMIC_UPDATE_OPT_BODY);
    set_str_opt(response_body, &opts.response_body,
                &flags, NGX_DYNAMIC_UPDATE_OPT_RESPONSE_BODY);
    set_num_opt<ngx_int_t>(off, &opts.off, &flags, NGX_DYNAMIC_UPDATE_OPT_OFF);

    if (!response_codes->not_found) {
        opts.response_codes.data = (ngx_int_t *) ngx_pcalloc(r->pool,
            20 * sizeof(ngx_int_t));
        if (opts.response_codes.data == NULL)
            return NGX_ERROR;
        opts.response_codes.reserved = 20;
        for (s = response_codes->data;
             s < response_codes->data + response_codes->len
                 && opts.response_codes.len < 20;
             s = c + 1) {
            for (c = s;
                 *c != '|' && c < response_codes->data + response_codes->len;
                 c++);
            opts.response_codes.data[opts.response_codes.len++] =
                ngx_atoi(s, c - s);
        }
        flags |= NGX_DYNAMIC_UPDATE_OPT_RESPONSE_CODES;
    }

    if (!request_headers->not_found) {
        opts.request_headers.data = (ngx_keyval_t *) ngx_pcalloc(r->pool,
            100 * sizeof(ngx_keyval_t));
        if (opts.request_headers.data == NULL)
            return NGX_ERROR;
        opts.request_headers.reserved = 100;
        for (s = request_headers->data;
             s < request_headers->data + request_headers->len
                 && opts.request_headers.len < 100;
             s = c + 1) {
            for (c = s;
                 *c != '|' && c < request_headers->data + request_headers->len;
                 c++);
            ngx_keyval_t kv;
            kv.key.data = s;
            kv.key.len = (u_char *) ngx_strchr(kv.key.data, ':') - kv.key.data;
            kv.key.data[kv.key.len] = 0;
            kv.value.data = kv.key.data + kv.key.len + 1;
            kv.value.len = c - kv.value.data;
            kv.value.data[kv.value.len] = 0;
            opts.request_headers.data[opts.request_headers.len++] = kv;
        }
        flags |= NGX_DYNAMIC_UPDATE_OPT_HEADERS;
    }

    if (flags) {
        rc = ngx_dynamic_healthcheck_update(&opts, flags);
        if (rc != NGX_OK && rc != NGX_DECLINED)
            return rc;
    }

    if (!disable_host->not_found && disable_host->len != 0) {
        ngx_str_t host = { disable_host->len, disable_host->data };
        rc = ngx_dynamic_healthcheck_disable_host(opts.module, opts.upstream,
                                                  host, 1);
        if (rc != NGX_OK && rc != NGX_DECLINED)
            return rc;
    }

    if (!enable_host->not_found && enable_host->len != 0) {
        ngx_str_t host = { enable_host->len, enable_host->data };
        rc = ngx_dynamic_healthcheck_disable_host(opts.module, opts.upstream,
                                                  host, 0);
        if (rc != NGX_OK && rc != NGX_DECLINED)
            return rc;
    }

    if (!disable->not_found)
        rc = ngx_dynamic_healthcheck_disable(opts.module, opts.upstream,
                                             ngx_atoi(disable->data,
                                                      disable->len));

    return rc;
}


static ngx_int_t
ngx_http_dynamic_healthcheck_update_handler(ngx_http_request_t *r)
{
    static ngx_str_t            text = ngx_string("text/plain");
    ngx_chain_t                 out;
    ngx_int_t                   rc;

    if (r->method != NGX_HTTP_GET)
        return NGX_HTTP_NOT_ALLOWED;

    if ((rc = ngx_http_discard_request_body(r)) != NGX_OK)
        return rc;

    out.buf = ngx_create_temp_buf(r->pool, ngx_pagesize);
    if (out.buf == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    out.next = NULL;

    out.buf->last_buf = (r == r->main) ? 1: 0;
    out.buf->last_in_chain = 1;

    rc = ngx_http_dynamic_healthcheck_update(r);

    switch (rc) {
        case NGX_OK:
            r->headers_out.status = NGX_HTTP_OK;
            out.buf->last = ngx_snprintf(out.buf->last,
                                         out.buf->end - out.buf->last,
                                         "updated");
            break;

        case NGX_DECLINED:
            r->headers_out.status = NGX_HTTP_NOT_MODIFIED;
            r->header_only = 1;
            break;

        case NGX_AGAIN:
            r->headers_out.status = NGX_HTTP_BAD_REQUEST;
            out.buf->last = ngx_snprintf(out.buf->last,
                                         out.buf->end - out.buf->last,
                                         "bad request");
            break;

        case NGX_ERROR:
        default:
            r->headers_out.status = NGX_HTTP_INTERNAL_SERVER_ERROR;
            out.buf->last = ngx_snprintf(out.buf->last,
                                         out.buf->end - out.buf->last,
                                         "internal error");
            break;
    }

    if (!r->header_only) {
        r->headers_out.content_type = text;
        r->headers_out.content_length_n = out.buf->last - out.buf->start;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK)
        return rc;

    if (r->header_only)
        return NGX_OK;

    return ngx_http_output_filter(r, &out);
}


static ngx_str_t peers_desc[2] = {
    ngx_string("primary"),
    ngx_string("backup")
};


template <class S, class PeersT, class PeerT> ngx_chain_t *
ngx_http_dynamic_healthcheck_status_hc(ngx_http_request_t *r,
    ngx_dynamic_healthcheck_conf_t *conf, ngx_str_t tab)
{
    S                      *uscf;
    PeersT                 *primary, *peers;
    PeerT                  *peer;
    ngx_uint_t              i;
    ngx_dynamic_hc_stat_t   stat;
    ngx_chain_t            *out;
    
    out = (ngx_chain_t *) ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
    if (out == NULL)
        return NULL;
    
    out->buf = ngx_create_temp_buf(r->pool, ngx_pagesize);
    if (out->buf == NULL)
        return NULL;

    if (conf != NULL) {
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "{" CRLF,
                                      &conf->shared->upstream);

        uscf = (S *) conf->uscf;

        primary = (PeersT *) uscf->peer.data;
        peers = primary;

        ngx_rwlock_rlock(&primary->rwlock);

        for (i = 0; peers && i < 2; peers = peers->next, i++) {
            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last,
                                          "%V    \"%V\":{" CRLF,
                                          &tab,
                                          &peers_desc[i]);

            for (peer = peers->peer; peer; peer = peer->next) {
                if (ngx_dynamic_healthcheck_state_stat(&conf->peers,
                        &peer->server, &peer->name, &stat) != NGX_OK)
                    ngx_memzero(&stat, sizeof(ngx_dynamic_hc_stat_t));

                out->buf->last = ngx_snprintf(out->buf->last,
                    out->buf->end - out->buf->last,
                    "%V        \"%V\":{"  CRLF, &tab, &peer->name);

                // state
                out->buf->last = ngx_snprintf(out->buf->last,
                    out->buf->end - out->buf->last,
                    "%V            \"down\":%d,"        CRLF
                    "%V            \"fall\":%d,"        CRLF
                    "%V            \"rise\":%d,"        CRLF
                    "%V            \"fall_total\":%d,"  CRLF
                    "%V            \"rise_total\":%d"   CRLF,
                        &tab, peer->down,
                        &tab, stat.fall,
                        &tab, stat.rise,
                        &tab, stat.fall_total,
                        &tab, stat.rise_total);

                out->buf->last = ngx_snprintf(out->buf->last,
                    out->buf->end - out->buf->last,
                    "%V        }", &tab);

                if (peer->next != NULL)
                    out->buf->last = ngx_snprintf(out->buf->last,
                        out->buf->end - out->buf->last, ",");
                out->buf->last = ngx_snprintf(out->buf->last,
                    out->buf->end - out->buf->last, CRLF);
            }

            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last,
                                          "%V    }",
                                          &tab);
            if (i == 0 && peers->next)
                out->buf->last = ngx_snprintf(out->buf->last,
                                              out->buf->end - out->buf->last,
                                              ",",
                                              &conf->shared->upstream);
            out->buf->last = ngx_snprintf(out->buf->last,
                                          out->buf->end - out->buf->last,
                                          CRLF,
                                          &conf->shared->upstream);
        }

        ngx_rwlock_unlock(&primary->rwlock);

        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last, "%V}",
                                      &tab, &conf->shared->upstream);
    }

    return out;
}


template <class M, class S, class PeersT, class PeerT> ngx_chain_t *
ngx_http_dynamic_healthcheck_status(ngx_http_request_t *r,
    ngx_http_variable_value_t *upstream)
{
    S                               **uscf;
    M                                *umcf = NULL;
    ngx_dynamic_healthcheck_conf_t   *conf;
    ngx_chain_t                      *start = NULL, *out = NULL, *next = NULL;
    ngx_uint_t                        i;
    ngx_str_t                         tab = no_tab;

    start = out = (ngx_chain_t *) ngx_pcalloc(r->pool, sizeof(ngx_chain_t));
    if (start == NULL)
        return NULL;

    start->buf = ngx_create_temp_buf(r->pool, ngx_pagesize);
    if (start->buf == NULL)
        return NULL;

    umcf = ngx_dynamic_healthcheck_api_base::get_upstream_conf(umcf);

    if (umcf == NULL || umcf->upstreams.nelts == 0)
        goto skip;

    uscf = (S **) umcf->upstreams.elts;

    if (upstream->not_found) {
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "{"CRLF);
        tab = with_tab;
    }

    for (i = 0; i < umcf->upstreams.nelts; i++) {

        conf = ngx_dynamic_healthcheck_api_base::get_srv_conf(uscf[i]);
        if (conf == NULL)
            continue;

        if (conf->shared->type.len == 0)
            continue;

        if (upstream->not_found
            || ngx_memn2cmp(upstream->data, conf->shared->upstream.data,
                            upstream->len, conf->shared->upstream.len) == 0) {
            if (upstream->not_found)
                out->buf->last = ngx_snprintf(out->buf->last,
                                              out->buf->end - out->buf->last,
                                              "    \"%V\":",
                                              &conf->shared->upstream);

            next = ngx_http_dynamic_healthcheck_status_hc
                    <S, PeersT, PeerT>(r, conf, tab);
            if (next == NULL)
                return NULL;

            if (upstream->not_found && i != umcf->upstreams.nelts - 1)
                next->buf->last = ngx_snprintf(next->buf->last,
                                               next->buf->end - next->buf->last,
                                               ",");
            next->buf->last = ngx_snprintf(next->buf->last,
                                           next->buf->end - next->buf->last,
                                               CRLF);

            out->next = next;
            out = next;
        }
    }

    if (upstream->not_found)
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last, "}"CRLF);

skip:

    if (umcf == NULL || umcf->upstreams.nelts == 0)
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "{}"CRLF);

    out->buf->last_buf = (r == r->main) ? 1: 0;
    out->buf->last_in_chain = 1;

    if (!upstream->not_found && start->next == NULL) {
        // upstream not found
        start->buf->last = start->buf->start;
        return start;
    }

    return upstream->not_found ? start : start->next;
}


static ngx_int_t
ngx_http_dynamic_healthcheck_status_handler(ngx_http_request_t *r)
{
    static ngx_str_t            json = ngx_string("application/json");
    ngx_chain_t                *out = NULL, *tmp;
    ngx_int_t                   rc;
    ngx_http_variable_value_t  *upstream;
    ngx_http_variable_value_t  *stream;
    off_t                       content_length = 0;

    if (r->method != NGX_HTTP_GET)
        return NGX_HTTP_NOT_ALLOWED;

    if ((rc = ngx_http_discard_request_body(r)) != NGX_OK)
        return rc;

    upstream = get_arg(r, "arg_upstream");
    stream = get_arg(r, "arg_stream");

    out = stream->not_found
        ? ngx_http_dynamic_healthcheck_status
            <ngx_http_upstream_main_conf_t,
             ngx_http_upstream_srv_conf_t,
             ngx_http_upstream_rr_peers_t,
             ngx_http_upstream_rr_peer_t> (r, upstream)
        : ngx_http_dynamic_healthcheck_status
            <ngx_stream_upstream_main_conf_t,
             ngx_stream_upstream_srv_conf_t,
             ngx_stream_upstream_rr_peers_t,
             ngx_stream_upstream_rr_peer_t>(r, upstream);

    if (out == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    for (tmp = out; tmp; tmp = tmp->next)
        content_length += (tmp->buf->last - tmp->buf->start);

    if (content_length != 0) {
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_type = json;
        r->headers_out.content_length_n = content_length;
    } else {
        r->headers_out.status = NGX_HTTP_NOT_FOUND;
        out->buf->last = ngx_snprintf(out->buf->last,
                                      out->buf->end - out->buf->last,
                                      "not found");
        r->headers_out.content_length_n = 9;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK)
        return rc;

    return ngx_http_output_filter(r, out);
}


static char *
ngx_http_dynamic_healthcheck_get(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = (ngx_http_core_loc_conf_t *) ngx_http_conf_get_module_loc_conf(cf,
        ngx_http_core_module);
    clcf->handler = ngx_http_dynamic_healthcheck_get_handler;

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_healthcheck_update(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = (ngx_http_core_loc_conf_t *) ngx_http_conf_get_module_loc_conf(cf,
        ngx_http_core_module);
    clcf->handler = ngx_http_dynamic_healthcheck_update_handler;

    return NGX_CONF_OK;
}


static char *
ngx_http_dynamic_healthcheck_status(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = (ngx_http_core_loc_conf_t *) ngx_http_conf_get_module_loc_conf(cf,
        ngx_http_core_module);
    clcf->handler = ngx_http_dynamic_healthcheck_status_handler;

    return NGX_CONF_OK;
}

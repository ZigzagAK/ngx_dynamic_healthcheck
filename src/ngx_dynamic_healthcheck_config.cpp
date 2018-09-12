/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

extern "C" {
#include <ngx_core.h>
#include <ngx_string.h>
}


#include "ngx_dynamic_healthcheck_config.h"


extern ngx_str_t NGX_DH_MODULE_HTTP;

ngx_inline int
ngx_is_arg(const char *n, ngx_str_t arg)
{
    return arg.len > ngx_strlen(n) &&
        ngx_strncmp(arg.data, n, ngx_strlen(n)) == 0;
}


char *
ngx_dynamic_healthcheck_check(ngx_conf_t *cf, ngx_command_t *cmd,
    void *p)
{
    ngx_str_t *value;
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_uint_t i;
    ngx_str_t arg, type;

    conf = (ngx_dynamic_healthcheck_conf_t *) p;

    value = (ngx_str_t *) cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        arg = value[i];

        if (ngx_is_arg("type=", arg)) {
            type.data = arg.data + 5;
            type.len = arg.len - 5;

            if (conf->config.module.data == NGX_DH_MODULE_HTTP.data) {
                if (ngx_strncmp("http", type.data, type.len) != 0
                    && ngx_strncmp("tcp", type.data, type.len) != 0
                    && ngx_strncmp("ssl", type.data, type.len) != 0)
                goto fail;
            } else {
                if (ngx_strncmp("tcp", type.data, type.len) != 0
                    && ngx_strncmp("ssl", type.data, type.len) != 0)
                goto fail;
            }

            conf->config.type = type;

            continue;
        }

        if (ngx_is_arg("timeout=", arg)) {
            conf->config.timeout = ngx_atoi(arg.data + 8, arg.len - 8);

            if (conf->config.timeout < 1)
                goto fail;

            continue;
        }

        if (ngx_is_arg("rise=", arg)) {
            conf->config.rise = ngx_atoi(arg.data + 5, arg.len - 5);

            if (conf->config.rise < 1)
                goto fail;

            continue;
        }
        
        if (ngx_is_arg("fall=", arg)) {
            conf->config.fall = ngx_atoi(arg.data + 5, arg.len - 5);

            if (conf->config.fall < 1)
                goto fail;

            continue;
        }

        if (ngx_is_arg("interval=", arg)) {
            conf->config.interval = ngx_atoi(arg.data + 9, arg.len - 9);

            if (conf->config.interval < 0)
                goto fail;

            continue;
        }

        if (ngx_is_arg("keepalive=", arg) &&
            conf->config.module.data == NGX_DH_MODULE_HTTP.data) {
            conf->config.keepalive = ngx_atoi(arg.data + 10, arg.len - 10);

            if (conf->config.keepalive == 0)
                goto fail;

            continue;
        }

        if (ngx_strcmp(arg.data, "off") == 0) {
            conf->config.off = 1;
            continue;
        }
    }

    if (conf->config.type.len == 0) {
        conf->config.type.data = (u_char *) "tcp";
        conf->config.type.len = 3;
    }
    
    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
        "ngx_dynamic_helthcheck: invalid parameter '%V'", &arg);

    return (char *) NGX_CONF_ERROR;
}


char *
ngx_http_dynamic_healthcheck_check_request_uri(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p)
{
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_str_t                      *value;

    conf = (ngx_dynamic_healthcheck_conf_t *) p;
    value = (ngx_str_t *) cf->args->elts;

    conf->config.request_method = value[1];
    conf->config.request_uri = value[2];

    return NGX_CONF_OK;
}


char *
ngx_http_dynamic_healthcheck_check_request_headers(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p)
{
    ngx_str_t                      *value;
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_dynamic_healthcheck_opts_t *opts;
    const char                     *sep;
    ngx_uint_t                      i;

    conf = (ngx_dynamic_healthcheck_conf_t *) p;

    value = (ngx_str_t *) cf->args->elts;
    opts = &conf->config;

    opts->request_headers.reserved = cf->args->nelts - 1;
    opts->request_headers.len = cf->args->nelts - 1;
    opts->request_headers.data = (ngx_keyval_t *) ngx_pcalloc(cf->pool,
        opts->request_headers.len * sizeof(ngx_keyval_t));

    if (opts->request_headers.data == NULL)
        return NULL;

    for (i = 1; i < cf->args->nelts; ++i) {
        sep = ngx_strchr(value[i].data, '=');
        if (sep == NULL)
            goto fail;

        opts->request_headers.data[i - 1].key.len =
            (u_char *) sep - value[i].data;
        opts->request_headers.data[i - 1].key.data = value[i].data;

        opts->request_headers.data[i - 1].value.len =
            (value[i].data + value[i].len - (u_char *) sep) - 1;
        opts->request_headers.data[i - 1].value.data = (u_char *) sep + 1;
    }

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid header desc '%V'", &value[i]);

    return (char *) NGX_CONF_ERROR;
}


char *
ngx_http_dynamic_healthcheck_check_response_codes(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p)
{
    ngx_str_t                      *value;
    ngx_dynamic_healthcheck_conf_t *conf;
    ngx_dynamic_healthcheck_opts_t *opts;
    ngx_uint_t                      i;

    conf = (ngx_dynamic_healthcheck_conf_t *) p;

    value = (ngx_str_t *) cf->args->elts;
    opts = &conf->config;

    opts->response_codes.reserved = cf->args->nelts - 1;
    opts->response_codes.len = cf->args->nelts - 1;
    opts->response_codes.data = (ngx_int_t *) ngx_pcalloc(cf->pool,
        opts->response_codes.len * sizeof(ngx_int_t));

    if (opts->response_codes.data == NULL)
        return NULL;

    for (i = 1; i < cf->args->nelts; ++i) {
        opts->response_codes.data[i - 1] =
            ngx_atoi(value[i].data, value[i].len);
        if (opts->response_codes.data[i - 1] < 1)
            goto fail;
    }

    return NGX_CONF_OK;

fail:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid response code '%V'", &value[i]);

    return (char *) NGX_CONF_ERROR;
}


char *
ngx_conf_set_str_array_slot2(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char  *p = (char *) conf;

    ngx_str_t         *value, *s;
    ngx_str_array_t   *a;
    ngx_conf_post_t   *post;

    a = (ngx_str_array_t *) (p + cmd->offset);

    if (a->data == NGX_CONF_UNSET_PTR) {
        a->data = (ngx_str_t *) ngx_pcalloc(cf->pool, 100 * sizeof(ngx_str_t));
        if (a->data == NULL)
            return (char *) NGX_CONF_ERROR;
        a->reserved = 100;
    }

    if (a->len == a->reserved)
        return (char *) NGX_CONF_ERROR;
    
    s = a->data + a->len;

    value = (ngx_str_t *) cf->args->elts;

    *s = value[1];

    a->len++;

    if (cmd->post) {
        post = (ngx_conf_post_t *) cmd->post;
        return post->post_handler(cf, post, s);
    }

    return (char *) NGX_CONF_OK;
}
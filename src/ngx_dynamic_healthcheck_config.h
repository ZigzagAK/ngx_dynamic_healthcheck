/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_CONFIG_H
#define NGX_DYNAMIC_HEALTHCHECK_CONFIG_H

#include "ngx_dynamic_healthcheck.h"
#include "ngx_dynamic_shm.h"


// common

char *
ngx_conf_set_str_array_slot2(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

char *
ngx_dynamic_healthcheck_check(ngx_conf_t *cf, ngx_command_t *cmd,
    void *p);

// http

char *
ngx_http_dynamic_healthcheck_check_request_uri(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p);

char *
ngx_http_dynamic_healthcheck_check_request_headers(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p);

char *
ngx_http_dynamic_healthcheck_check_response_codes(ngx_conf_t *cf,
    ngx_command_t *cmd, void *p);

// macros

#ifdef ngx_conf_merge_str_value
#undef ngx_conf_merge_str_value
#endif

#define ngx_conf_merge_str_value(conf, prev)                                 \
    if (conf.data == NULL) {                                                 \
        if (prev.data) {                                                     \
            conf.len = prev.len;                                             \
            conf.data = prev.data;                                           \
        }                                                                    \
    }

#define ngx_conf_merge_array_value(conf, prev)                               \
    if (conf.data == NGX_CONF_UNSET_PTR) {                                   \
        if (prev.data) {                                                     \
            conf.len = prev.len;                                             \
            conf.data = prev.data;                                           \
            conf.reserved = prev.reserved;                                   \
        }                                                                    \
    }

#ifdef __cplusplus
#undef NGX_CONF_ERROR
#define NGX_CONF_ERROR (char *) -1
#endif

#endif /* CONFIG_H */


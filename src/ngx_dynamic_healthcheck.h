/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_H
#define NGX_DYNAMIC_HEALTHCHECK_H


#ifdef __cplusplus
extern "C" {
#endif

#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>
#include <ngx_stream.h>

#ifdef __cplusplus
}
#endif

#include "ngx_dynamic_healthcheck_state.h"

struct ngx_str_array_s {
    ngx_str_t   *data;
    ngx_uint_t   len;
    ngx_uint_t   reserved;
};
typedef struct ngx_str_array_s ngx_str_array_t;


struct ngx_keyval_array_s {
    ngx_keyval_t *data;
    ngx_uint_t    len;
    ngx_uint_t    reserved;
};
typedef struct ngx_keyval_array_s ngx_keyval_array_t;


struct ngx_num_array_s {
    ngx_int_t    *data;
    ngx_uint_t    len;
    ngx_uint_t    reserved;
};
typedef struct ngx_num_array_s ngx_num_array_t;


struct ngx_dynamic_healthcheck_opts_s {
    ngx_str_t              module;
    ngx_str_t              upstream;
    ngx_str_t              type;
    ngx_int_t              fall;
    ngx_int_t              rise;
    ngx_int_t              timeout;
    ngx_int_t              interval;
    ngx_uint_t             keepalive;
    ngx_str_t              request_uri;
    ngx_str_t              request_method;
    ngx_keyval_array_t     request_headers;
    ngx_str_t              request_body;
    ngx_num_array_t        response_codes;
    ngx_str_t              response_body;
    ngx_flag_t             off;
    ngx_str_array_t        disabled_hosts_global;
    ngx_str_array_t        disabled_hosts;
    ngx_flag_t             disabled;
    size_t                 buffer_size;
    ngx_msec_t             last;
    ngx_dynamic_hc_state_t state;
    ngx_str_t              persistent;
    ngx_flag_t             updated;
    ngx_int_t              loaded;
};
typedef struct ngx_dynamic_healthcheck_opts_s
ngx_dynamic_healthcheck_opts_t;


struct ngx_dynamic_healthcheck_conf_s;


typedef void (*ngx_shm_zone_post_init_pt)
    (struct ngx_dynamic_healthcheck_conf_s *conf);


struct ngx_dynamic_healthcheck_conf_s {
    ngx_dynamic_healthcheck_opts_t  config;
    ngx_dynamic_healthcheck_opts_t *shared;
    ngx_event_t                     event;
    ngx_shm_zone_t                 *zone;
    ngx_shm_zone_post_init_pt       post_init;
    void                           *uscf;
};
typedef struct ngx_dynamic_healthcheck_conf_s
ngx_dynamic_healthcheck_conf_t;


ngx_int_t
ngx_dynamic_healthcheck_init_worker(ngx_cycle_t *cycle);


struct ngx_dynamic_healthcheck_event_s;

typedef void (*ngx_dynamic_healthcheck_event_completed_pt)
    (struct ngx_dynamic_healthcheck_event_s *event);

struct ngx_dynamic_healthcheck_event_s {
    ngx_connection_t                            dumb_conn;
    void                                       *uscf;
    ngx_int_t                                   remains;
    ngx_flag_t                                  in_progress;
    ngx_log_t                                  *log;
    ngx_dynamic_healthcheck_conf_t             *conf;
    ngx_dynamic_healthcheck_event_completed_pt  completed;
};
typedef struct ngx_dynamic_healthcheck_event_s ngx_dynamic_healthcheck_event_t;


#ifdef __cplusplus

class ngx_dynamic_event_handler_base {

public:

    static ngx_int_t
    do_check(ngx_http_upstream_srv_conf_t *uscf,
        ngx_dynamic_healthcheck_event_t *event);

    static ngx_int_t
    do_check(ngx_stream_upstream_srv_conf_t *uscf,
        ngx_dynamic_healthcheck_event_t *event);
};


template <class S> class ngx_dynamic_event_handler :
    private ngx_dynamic_event_handler_base {

public:

    static void
    check(ngx_event_t *ev)
    {
        ngx_dynamic_healthcheck_event_t *event;
        S                               *uscf;

        event = (ngx_dynamic_healthcheck_event_t *) ev->data;
        uscf = (S *) event->uscf;

        event->log = ev->log;

        if (!event->in_progress) {
            if (ngx_dynamic_event_handler_base::do_check(uscf, event)
                    != NGX_OK)
                goto end;

            event->in_progress = 1;

            goto timer;
        }

        if (event->remains == 0)
            goto end;

timer:
        ngx_add_timer(ev, 1000);

        return;

end:

        event->completed(event);

        ngx_shmtx_lock(&event->conf->shared->state.slab->mutex);

        ngx_memzero(ev, sizeof(ngx_event_t));

        ngx_shmtx_unlock(&event->conf->shared->state.slab->mutex);

        ngx_free(event);
    }
};


#endif


#endif

#include <ngx_core.h>
#include <ngx_http.h>

ngx_int_t
ngx_http_read_header(ngx_http_request_t *r, ngx_buf_t *buf, ngx_keyval_t *h)
{
    switch (ngx_http_parse_header_line(r, buf, 1)) {
        case NGX_OK:
            break;

        case NGX_AGAIN:
            return NGX_AGAIN;

        case NGX_HTTP_PARSE_HEADER_DONE:
            return NGX_HTTP_PARSE_HEADER_DONE;

        case NGX_HTTP_PARSE_INVALID_HEADER:
            return NGX_DECLINED;

        case NGX_ERROR:
        default:
            return NGX_ERROR;
    }

    if (r->header_name_end == r->header_name_start)
        return NGX_DECLINED;
    
    h->key.len = r->header_name_end - r->header_name_start;
    h->key.data = r->header_name_start;
    h->key.data[h->key.len] = 0;

    h->value.len = r->header_end - r->header_start;
    h->value.data = r->header_start;
    h->value.data[h->value.len] = 0;

    ngx_strlow(h->key.data, h->key.data, h->key.len);

    return NGX_OK;
}

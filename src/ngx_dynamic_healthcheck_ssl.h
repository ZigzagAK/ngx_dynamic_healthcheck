/*
 * Copyright (C) 2018 Aleksei Konovkin (alkon2000@mail.ru)
 */

#ifndef NGX_DYNAMIC_HEALTHCHECK_SSL_H
#define NGX_DYNAMIC_HEALTHCHECK_SSL_H

#include "ngx_dynamic_healthcheck_tcp.h"

#define NGX_SSL_RANDOM "............................"

/*
 * This is the SSLv3 CLIENT HELLO packet used in conjunction with the
 * check type of ssl_hello to ensure that the remote server speaks SSL.
 *
 * Check RFC 2246 (TLSv1.0) sections A.3 and A.4 for details.
 */
static char sslv3_client_hello_pkt[] = {
    "\x16"                /* ContentType         : 0x16 = Hanshake           */
    "\x03\x01"            /* ProtocolVersion     : 0x0301 = TLSv1.0          */
    "\x00\x6f"            /* ContentLength       : 0x6f bytes after this one */
    "\x01"                /* HanshakeType        : 0x01 = CLIENT HELLO       */
    "\x00\x00\x6b"        /* HandshakeLength     : 0x6b bytes after this one */
    "\x03\x03"            /* Hello Version       : 0x0303 = TLSv1.2          */
    "\x00\x00\x00\x00"    /* Unix GMT Time (s)   : filled with <now> (@0x0B) */
    NGX_SSL_RANDOM        /* Random              : must be exactly 28 bytes  */
    "\x00"                /* Session ID length   : empty (no session ID)     */
    "\x00\x1a"            /* Cipher Suite Length : \x1a bytes after this one */
    "\xc0\x2b" "\xc0\x2f" "\xcc\xa9" "\xcc\xa8"  /* 13 modern ciphers        */
    "\xc0\x0a" "\xc0\x09" "\xc0\x13" "\xc0\x14"
    "\x00\x33" "\x00\x39" "\x00\x2f" "\x00\x35"
    "\x00\x0a"
    "\x01"                /* Compression Length  : 0x01 = 1 byte for types   */
    "\x00"                /* Compression Type    : 0x00 = NULL compression   */
    "\x00\x28"            /* Extensions length */
    "\x00\x0a"            /* EC extension */
    "\x00\x08"            /* extension length */
    "\x00\x06"            /* curves length */
    "\x00\x17" "\x00\x18" "\x00\x19" /* Three curves */
    "\x00\x0d"            /* Signature extension */
    "\x00\x18"            /* extension length */
    "\x00\x16"            /* hash list length */
    "\x04\x01" "\x05\x01" "\x06\x01" "\x02\x01"  /* 11 hash algorithms */
    "\x04\x03" "\x05\x03" "\x06\x03" "\x02\x03"
    "\x05\x02" "\x04\x02" "\x02\x02"
};


#define NGX_SSL_HANDSHAKE    0x16
#define NGX_SSL_SERVER_HELLO 0x02


#pragma pack(push, 1)

typedef struct {
    u_char  major;
    u_char  minor;
} ngx_ssl_protocol_version_t;


typedef struct {
    u_char                      msg_type;
    ngx_ssl_protocol_version_t  version;
    uint16_t                    length;

    u_char                      handshake_type;
    u_char                      handshake_length[3];
    ngx_ssl_protocol_version_t  hello_version;

    time_t                      time;
    u_char                      random[28];

    u_char                      others[0];
} ngx_ssl_server_hello_t;

#pragma pack()


template <class PeerT> class ngx_dynamic_healthcheck_ssl :
    public ngx_dynamic_healthcheck_tcp<PeerT>
{
protected:

    ngx_int_t
    make_request(ngx_dynamic_hc_local_node_t *state)
    {
        static char  alphabet[] = "1234567890abcdefghijklmnopqrstuvwxyz";

        ngx_uint_t   i;
        ngx_str_t    s;

        for(i = 0; i < 28; i++)
            sslv3_client_hello_pkt[15 + i] =
                alphabet[ngx_random() % sizeof(alphabet)];

        s.data = (u_char *) sslv3_client_hello_pkt;
        s.len = sizeof(sslv3_client_hello_pkt);

        state->buf->last = ngx_snprintf(state->buf->start,
            state->buf->end - state->buf->start, "%V", &s);

        return NGX_OK;
    }

    virtual ngx_int_t
    on_send(ngx_dynamic_hc_local_node_t *state)
    {
        if (state->buf->last == state->buf->start)
            if (make_request(state) == NGX_ERROR)
                return NGX_ERROR;

        return ngx_dynamic_healthcheck_tcp<PeerT>::on_send(state);
    }

    virtual ngx_int_t
    on_recv(ngx_dynamic_hc_local_node_t *state)
    {
        ngx_buf_t               *buf = state->buf;
        ngx_connection_t        *c = state->pc.connection;
        ssize_t                  size;
        ngx_ssl_server_hello_t  *hello;

        size = c->recv(c, buf->last, buf->end - buf->last);

        ngx_log_debug6(NGX_LOG_DEBUG_HTTP, c->log, 0,
                       "[%V] %V: %V addr=%V, "
                       "fd=%d on_recv() recv: %d",
                       &this->module, &this->upstream,
                       &this->server, &this->name, c->fd,
                       size);

        if (size == NGX_ERROR)
            return NGX_ERROR;

        if (size == NGX_AGAIN)
            return NGX_AGAIN;

        buf->last += size;

        if (buf->last - buf->start < (ngx_int_t) sizeof(ngx_ssl_server_hello_t))
            return NGX_AGAIN;

        hello = (ngx_ssl_server_hello_t *) buf->start;

        ngx_log_debug(NGX_LOG_DEBUG_HTTP, c->log, 0,
                      "[%V] %V: %V addr=%V, "
                      "fd=%d ssl on_recv(): type: %ud, version: %ud.%ud, "
                      "length: %ud, handshanke_type: "
                      "%ud, hello_version: %ud.%ud",
                      &this->module, &this->upstream,
                      &this->server, &this->name, c->fd,
                      hello->msg_type,
                      hello->version.major, hello->version.minor,
                      ntohs(hello->length), hello->handshake_type,
                      hello->hello_version.major, hello->hello_version.minor);

        if (hello->msg_type != NGX_SSL_HANDSHAKE)
            return NGX_ERROR;

        if (hello->handshake_type != NGX_SSL_SERVER_HELLO)
            return NGX_ERROR;

        return NGX_OK;
    }
    
public:

    ngx_dynamic_healthcheck_ssl(PeerT *peer,
        ngx_dynamic_healthcheck_event_t *event, ngx_dynamic_hc_state_node_t s)
        : ngx_dynamic_healthcheck_tcp<PeerT>(peer, event, s)
    {}
};

#endif /* NGX_DYNAMIC_HEALTHCHECK_SSL_H */


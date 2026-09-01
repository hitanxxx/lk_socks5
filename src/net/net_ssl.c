#include "common.h"
#include "net_event.h"
#include "net_event_timer.h"
#include "net_ssl.h"

typedef struct {
    SSL_CTX *ctx_client;
    SSL_CTX *ctx_server;
    char g_err_msg[1024];
} g_ssl_t;

static g_ssl_t *g_ssl_ctx = NULL;

#define ssl_clear_error()                                                      \
    {                                                                          \
        unsigned long rc = 0;                                                  \
        do {                                                                   \
            rc = ERR_get_error();                                              \
        } while (rc);                                                          \
        ERR_clear_error();                                                     \
    }

#define ssl_dump_error(sslerr)                                                 \
    {                                                                          \
        unsigned long n = 0;                                                   \
        unsigned char errstr[512] = {0};                                       \
        unsigned char *p = errstr;                                             \
        unsigned char *last = errstr + sizeof(errstr);                         \
        if (ERR_peek_error()) {                                                \
            while ((n = ERR_peek_error())) {                                   \
                if (p < last - 1) {                                            \
                    ERR_error_string_n(n, (char *)p, last - p);                \
                }                                                              \
                (void)ERR_get_error();                                         \
            }                                                                  \
            err("ssl err. [%s]\n", errstr);                                    \
        }                                                                      \
    }

int net_ssl_check_handshaked(con_t *c) {
    if (c->ssl && c->ssl->f_handshaked) return 1;
    return 0;
}

int net_ssl_check_err(con_t *c) {
    if(c->ssl && c->ssl->f_err) return 1;
    return 0;
}

static int net_ssl_read_cb(con_t *c) { return c->read_cb(c); }

static int net_ssl_read(con_t *c, uint8_t *buf, uint32_t bufn) {
    net_ssl_t *sslc = c->ssl;

    ssl_clear_error();
    int rc = SSL_read(sslc->con, buf, bufn);
    if (rc > 0) {
        if (sslc->cached_mask) {
            net_ev_set(c, c->ssl->cached_mask);
            c->read_cb = c->ssl->cached_readcb;
            c->write_cb = c->ssl->cached_writecb;
            sslc->cached_mask = 0;
        }
        return rc;
    }

    int sslerr = SSL_get_error(sslc->con, rc);
    if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if (!sslc->cached_mask) {
            sslc->cached_mask = c->ev->mask;
            sslc->cached_readcb = c->read_cb;
            sslc->cached_writecb = c->write_cb;
        }

        if (sslerr == SSL_ERROR_WANT_READ) {
            net_ev_set(c, EV_R);
        } else if (sslerr == SSL_ERROR_WANT_WRITE) {
            c->write_cb = net_ssl_read_cb;
            net_ev_set(c, EV_W);
        }
        return -11;
    }
    
    if (sslerr == SSL_ERROR_ZERO_RETURN) {   ///peer send close_notify
        sslc->f_closed = 1;
        return -1;
    } else if (sslerr == SSL_ERROR_SYSCALL) {
        if (errno != 0) {
            err("ssl syscall err. [%d]\n", errno);
        }
        sslc->f_err = 1;
        return -1;
    }
    sslc->f_err = 1;
    ssl_dump_error(sslerr);
    return -1;
}

static int net_ssl_write_cb(con_t *c) { return c->write_cb(c); }

static int net_ssl_write(con_t *c, uint8_t *data, uint32_t datan) {
    net_ssl_t *sslc = c->ssl;

    ssl_clear_error();
    int rc = SSL_write(sslc->con, data, datan);
    if (rc > 0) {
        if (sslc->cached_mask) {
            net_ev_set(c, c->ssl->cached_mask);
            c->read_cb = c->ssl->cached_readcb;
            c->write_cb = c->ssl->cached_writecb;
            sslc->cached_mask = 0;
        }
        return rc;
    }

    int sslerr = SSL_get_error(sslc->con, rc);
    if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if (!sslc->cached_mask) {
            sslc->cached_mask = c->ev->mask;
            sslc->cached_readcb = c->read_cb;
            sslc->cached_writecb = c->write_cb;
        }

        if (sslerr == SSL_ERROR_WANT_READ) {
            c->read_cb = net_ssl_write_cb;
            net_ev_set(c, EV_R);
        } else if (sslerr == SSL_ERROR_WANT_WRITE) {
            net_ev_set(c, EV_W);
        }
        return -11;
    }
    if (sslerr == SSL_ERROR_ZERO_RETURN) {
        err("ssl already closed\n");
    }
    
    sslc->f_err = 1;
    ssl_dump_error(sslerr);
    return -1;
}

static int net_ssl_write_chain(con_t *c, meta_t *meta) {
    int sendn;
    meta_t *cl = meta;

    for (;;) {
        for (cl = meta; cl; cl = cl->next) {
            if (meta_getlen(cl)) {
                break;
            }
        }
        if (!cl)
            return 1;

        sendn = net_ssl_write(c, cl->pos, meta_getlen(cl));
        if (sendn < 0) {
            if (-11 == sendn) {
                return -11;
            }
            err("ssl write err\n");
            return -1;
        }
        cl->pos += sendn;
    }
}

static int net_ssl_shutdown_cb(con_t *c) {
    int ret = net_ssl_shutdown(c);
    if (ret == -11) {
        return -11;
    }
    
    net_free_thorough(c);
    return 0;
}

int net_ssl_shutdown(con_t *c) {
    net_ssl_t *sslc = c->ssl;
    int sslerr = 0;
    int t = 0;
    int mode = 0;

    if (SSL_in_init(sslc->con)) {
        c->ssl->f_closed = 1;
        return 0;
    }

    if (sslc->f_err || sslc->f_handshakeing) {
        mode = SSL_RECEIVED_SHUTDOWN|SSL_SENT_SHUTDOWN;
        SSL_set_quiet_shutdown(sslc->con, 1);
    } else {
        mode = SSL_get_shutdown(sslc->con);
    }
    SSL_set_shutdown(sslc->con, mode);

    ssl_clear_error();
    for (t = 0; t < 2; t++) {
    
        int rc = SSL_shutdown(sslc->con);
        if (rc == 1) {
            c->ssl->f_closed = 1;
            return 0;
        }
        if (rc == 0 && t == 0) {
            continue;
        }
        sslerr = SSL_get_error(sslc->con, rc);
        if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
            /// must clear read/write cb in here.
            /// make sure connection will not do
            /// another thing expect shutdown
            c->read_cb = c->write_cb = NULL;

            if (sslerr == SSL_ERROR_WANT_READ) {
                c->read_cb = net_ssl_shutdown_cb;
                net_ev_set(c, EV_R);
            } else {
                c->write_cb = net_ssl_shutdown_cb;
                net_ev_set(c, EV_W);
            } 
            return -11;
        } 

        /// SSL_ERROR_ZERO_RETURN
        /// SSL_ERROR_SYSCALL
        /// other error 
        sslc->f_closed = 1;
        return -1;
    }
    
    sslc->f_closed = 1;
    return -1;
}

static int net_ssl_handshake_cb(con_t *c) {
    int rc = net_ssl_handshake(c);
    if (rc == -11) {
        return -11;
    }

    net_timer_del(c);

    if (c->ssl->cached_mask) {
        net_ev_set(c, c->ssl->cached_mask);
        c->read_cb = c->ssl->cached_readcb;
        c->write_cb = c->ssl->cached_writecb;
        c->ssl->cached_mask = 0;
    }
    
    ///connect handshake or accept handshake 
    if (c->write_cb) {
        return c->write_cb(c);
    }
    return c->read_cb(c);
}

int net_ssl_handshake(con_t *c) {
    net_ssl_t *sslc = c->ssl;
    sslc->f_handshakeing = 1;

    ssl_clear_error();
    int rc = SSL_do_handshake(sslc->con);
    if (rc == 1) {
        if (sslc->cached_mask) {
            net_ev_set(c, c->ssl->cached_mask);
            c->read_cb = c->ssl->cached_readcb;
            c->write_cb = c->ssl->cached_writecb;
            sslc->cached_mask = 0;
        }

        sslc->f_handshakeing = 0;
        sslc->f_handshaked = 1;

        c->recv = net_ssl_read;
        c->send = net_ssl_write;
        c->send_chain = net_ssl_write_chain;
        return 0;
    }

    int sslerr = SSL_get_error(sslc->con, rc);
    if ((sslerr == SSL_ERROR_WANT_READ) || (sslerr == SSL_ERROR_WANT_WRITE)) {
        if (!sslc->cached_mask) {
            sslc->cached_mask = c->ev->mask;
            sslc->cached_readcb = c->read_cb;
            sslc->cached_writecb = c->write_cb;
        }

        if (sslerr == SSL_ERROR_WANT_READ) {
            c->read_cb = net_ssl_handshake_cb;
            net_ev_set(c, EV_R);
        } else if (sslerr == SSL_ERROR_WANT_WRITE) {
            c->write_cb = net_ssl_handshake_cb;
            net_ev_set(c, EV_W);
        }
        return -11;
    }

    ///SSL_ERROR_ZERO_RETURN
    ///SSL_ERROR_SYSCALL
    ///other error 
    if (sslerr == SSL_ERROR_SYSCALL) {
        if (errno != 0) err("ssl syscall err. [%d]\n", errno);
    }
    
    sslc->f_closed = 1;
    ssl_dump_error(sslerr);
    return -1;
}

static int net_ssl_create_con(SSL_CTX *ctx, int flag, SSL **ssl) {
    SSL *local_ssl = NULL;
    schk(local_ssl = SSL_new(ctx), return -1);
    *ssl = local_ssl;
    return 0;
}

static int net_ssl_create_ctx(SSL_CTX **ctx, int flag) {
    if (flag == L_SSL_CLIENT) {
        if (!g_ssl_ctx->ctx_client) {
            schk(g_ssl_ctx->ctx_client = SSL_CTX_new(TLS_client_method()), return -1);
            SSL_CTX_set_mode(g_ssl_ctx->ctx_client, SSL_MODE_AUTO_RETRY);
            SSL_CTX_set_mode(g_ssl_ctx->ctx_client, SSL_MODE_ENABLE_PARTIAL_WRITE);
            SSL_CTX_set_mode(g_ssl_ctx->ctx_client, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
            SSL_CTX_set_options(g_ssl_ctx->ctx_client, SSL_OP_IGNORE_UNEXPECTED_EOF);
            
            schk(1 == SSL_CTX_set_min_proto_version(g_ssl_ctx->ctx_client, TLS1_2_VERSION), return -1);
            schk(1 == SSL_CTX_set_max_proto_version(g_ssl_ctx->ctx_client, TLS1_3_VERSION), return -1);

            ///SSL_CTX_set_verify(g_ssl_ctx->ctx_client, SSL_VERIFY_PEER, NULL);
            ///SSL_CTX_set_default_verify_paths(g_ssl_ctx->ctx_client);
            SSL_CTX_set_session_cache_mode(g_ssl_ctx->ctx_client, SSL_SESS_CACHE_CLIENT);
        }
        *ctx = g_ssl_ctx->ctx_client;
    } else {
        if (!g_ssl_ctx->ctx_server) {
            int ret = -1;
            do {
                schk(g_ssl_ctx->ctx_server = SSL_CTX_new(TLS_server_method()), return -1);
                SSL_CTX_set_mode(g_ssl_ctx->ctx_server, SSL_MODE_AUTO_RETRY);
                SSL_CTX_set_mode(g_ssl_ctx->ctx_server, SSL_MODE_ENABLE_PARTIAL_WRITE);
                SSL_CTX_set_mode(g_ssl_ctx->ctx_server, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
                SSL_CTX_set_options(g_ssl_ctx->ctx_server, SSL_OP_IGNORE_UNEXPECTED_EOF);

                schk(1 == SSL_CTX_set_min_proto_version(g_ssl_ctx->ctx_server, TLS1_2_VERSION), return -1);
                schk(1 == SSL_CTX_set_max_proto_version(g_ssl_ctx->ctx_server, TLS1_3_VERSION), return -1);
                schk(1 == SSL_CTX_set_cipher_list(g_ssl_ctx->ctx_server,
                            "ECDHE-ECDSA-AES128-GCM-SHA256:"
                            "ECDHE-RSA-AES128-GCM-SHA256:"
                            "ECDHE-ECDSA-AES256-GCM-SHA384:"
                            "ECDHE-RSA-AES256-GCM-SHA384"), return -1);
                
                #if defined(__x86_64__)
                schk(1 == SSL_CTX_set_ciphersuites(g_ssl_ctx->ctx_server,
                            "TLS_AES_128_GCM_SHA256:"
                            "TLS_AES_256_GCM_SHA384:"
                            "TLS_CHACHA20_POLY1305_SHA256"), return -1);
                #elif defined(__arm__)
                ///tls 1.3
                schk(1 == SSL_CTX_set_ciphersuites(g_ssl_ctx->ctx_server,
                             "TLS_CHACHA20_POLY1305_SHA256"), return -1);
                ///tls 1.2
                schk(1 == SSL_CTX_set_cipher_list(g_ssl_ctx->ctx_server,
                            "ECDHE-ECDSA-AES128-GCM-SHA256:"
                            "ECDHE-RSA-AES128-GCM-SHA256:"
                            "ECDHE-ECDSA-AES256-GCM-SHA384:"
                            "ECDHE-RSA-AES256-GCM-SHA384"), break);
                #endif

                SSL_CTX_set_session_cache_mode(g_ssl_ctx->ctx_server, SSL_SESS_CACHE_SERVER);
                SSL_CTX_set_verify(g_ssl_ctx->ctx_server, SSL_VERIFY_NONE, NULL);
                schk(1 == SSL_CTX_use_certificate_file(g_ssl_ctx->ctx_server,
                                                        config_get()->ssl_crt_path,
                                                        SSL_FILETYPE_PEM),  break);
                schk(1 == SSL_CTX_use_PrivateKey_file( g_ssl_ctx->ctx_server,
                                                         config_get()->ssl_key_path,
                                                         SSL_FILETYPE_PEM), break);
                schk(SSL_CTX_check_private_key(g_ssl_ctx->ctx_server) == 1, break);
                ret = 0;
            } while (0);

            if (ret != 0) {
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return -1;
            }
        }
        *ctx = g_ssl_ctx->ctx_server;
    }
    return 0;
}

int net_ssl_create(con_t *c, int flag) {
    net_ssl_t *sslc = NULL;
    schk(((flag == L_SSL_SERVER) || (flag == L_SSL_CLIENT)), return -1);

    do {
        sslc = mem_pool_alloc(sizeof(net_ssl_t));
        schk(sslc, return -1);
        schk(net_ssl_create_ctx(&sslc->session_ctx, flag) == 0, break);
        schk(net_ssl_create_con(sslc->session_ctx, flag, &sslc->con) == 0, break);
        schk(SSL_set_fd(sslc->con, c->fd) != 0, break);
        if (flag == L_SSL_SERVER) {
            SSL_set_accept_state(sslc->con);
        } else {
            SSL_set_connect_state(sslc->con);
        }
        
        c->ssl = sslc;
        sslc->data = c;
        return 0;
    } while (0);

    if (sslc) {
        if (sslc->con) {
            SSL_free(sslc->con);
        }
        mem_pool_free(sslc);
    }
    return -1;
}

int net_ssl_init(void) {
    schk(!g_ssl_ctx, return -1);
    schk(g_ssl_ctx = mem_pool_alloc(sizeof(g_ssl_t)), return -1);

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return 0;
}

int net_ssl_exit(void) {
    ERR_free_strings();
    EVP_cleanup();
    if (g_ssl_ctx) {
        if (g_ssl_ctx->ctx_client) {
            SSL_CTX_free(g_ssl_ctx->ctx_client);
            g_ssl_ctx->ctx_client = NULL;
        }
        if (g_ssl_ctx->ctx_server) {
            SSL_CTX_free(g_ssl_ctx->ctx_server);
            g_ssl_ctx->ctx_server = NULL;
        }
        mem_pool_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
    return 0;
}

#include "common.h"


typedef struct
{
    SSL_CTX *  ctx_client;
    SSL_CTX *  ctx_server;
    char       g_err_msg[1024];
} g_ssl_t;

static g_ssl_t * g_ssl_ctx = NULL;

#define ssl_clear_error() \
{ \
    unsigned long rc = 0; \
    do { \
        rc = ERR_peek_error(); \
    } while(rc); \
    ERR_clear_error(); \
}

#define ssl_dump_error(sslerr) \
{ \
    unsigned long n = 0; \
    unsigned char errstr[1024] = {0}; \
    unsigned char *p = errstr, *last = errstr + sizeof(errstr); \
    if(ERR_peek_error()) { \
        while((n = ERR_peek_error())) { \
            if(p < last - 1) { \
                *p++ = ' '; \
                ERR_error_string_n(n, (char*)p, last-p); \
            } \
            (void)ERR_get_error(); \
        } \
        err("ssl err. [%s]\n", errstr); \
    } \
}


int ssl_shutdown_handler(event_t * ev)
{
    con_t * c = ev->data;
    event_pt cb = c->ssl->cb;
    timer_del(&ev->timer);

    int rc = ssl_shutdown(c->ssl);
    if(rc == -11) {
        timer_add(&ev->timer, L_SSL_TIMEOUT);
        return -11;
    }
    if(c->ssl && c->ssl->cache_ev_typ) {
        event_opt(ev, c->fd, c->ssl->cache_ev_typ);
        c->ssl->cache_ev_typ = 0;
    }
    return (cb ? cb(c->event) : rc);
}

int ssl_shutdown(ssl_con_t * ssl)
{
    con_t * c = ssl->data;
    int sslerr = 0;
    int try = 0;
    
    ssl_clear_error();

    for(try = 0; try < 2; try++) {
        int rc = SSL_shutdown(c->ssl->con);
        if(rc == 1) {
            return 0;
        }
        if(rc == 0 && try == 0) {
            continue;
        }
        sslerr = SSL_get_error(c->ssl->con, rc);
        if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
            c->ssl->f_shutdown_send = 1;
            if(!ssl->cache_ev_typ)
                ssl->cache_ev_typ = c->event->opt;

            if(sslerr == SSL_ERROR_WANT_READ) {
                c->event->read_pt = ssl_shutdown_handler;
                event_opt(c->event, c->fd, EV_R);
            } else {
                c->event->write_pt = ssl_shutdown_handler;
                event_opt(c->event, c->fd, EV_W);
            }
            return -11;
        }
    }
    ssl_dump_error(sslerr);
    return -1;
}


static int ssl_handshake_handler(event_t * ev)
{
    con_t * c = ev->data;
    timer_del(&ev->timer);
    
    int rc = ssl_handshake(c->ssl);
    if(rc == -11) {
        timer_add(&ev->timer, L_SSL_TIMEOUT);
        return -11;
    } 
    if(c->ssl && c->ssl->cache_ev_typ) {
        event_opt(ev, c->fd, c->ssl->cache_ev_typ);
        c->ssl->cache_ev_typ = 0;
    }
    return (c->ssl->cb ? c->ssl->cb(ev) : rc);
}

int ssl_handshake(ssl_con_t * ssl)
{
    con_t * c = ssl->data;
    
    ssl_clear_error();
    int rc = SSL_do_handshake(ssl->con);
    if(rc == 1) {
        ssl->f_handshaked = 1;
        return 0;
    }
    
    int sslerr = SSL_get_error(ssl->con, rc);
    if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if(!ssl->cache_ev_typ)
            ssl->cache_ev_typ = c->event->opt; ///cache current event opt
   
        if(sslerr == SSL_ERROR_WANT_READ) {
            c->event->read_pt = ssl_handshake_handler;
            event_opt(c->event, c->fd, EV_R);
        } else if (sslerr == SSL_ERROR_WANT_WRITE) {
            c->event->write_pt = ssl_handshake_handler;
            event_opt(c->event, c->fd, EV_W);
        }
        return -11;
    }
    ssl_dump_error(sslerr);
    return -1;
}

inline static int ssl_write_handler(event_t * ev)
{
    con_t * c = ev->data;
    if(c->ssl && c->ssl->cache_ev_writept) {
        c->event->write_pt = c->ssl->cache_ev_writept;
        c->ssl->cache_ev_writept = NULL;
    }
    return ev->read_pt(ev);
}

int ssl_read(con_t * c, unsigned char * buf, int bufn)
{
    ssl_clear_error();
    int rc = SSL_read(c->ssl->con, buf, bufn);
    if(rc > 0) {
        if(c->ssl->cache_ev_typ) {
            event_opt(c->event, c->fd, c->ssl->cache_ev_typ);
            c->ssl->cache_ev_typ = 0;
        }
        if(c->ssl->cache_ev_writept) {
            c->event->write_pt = c->ssl->cache_ev_writept;
            c->ssl->cache_ev_writept = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error(c->ssl->con, rc);
    if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if(!c->ssl->cache_ev_typ) c->ssl->cache_ev_typ = c->event->opt;
        if(!c->ssl->cache_ev_readpt) c->ssl->cache_ev_readpt = c->event->read_pt;
        if(!c->ssl->cache_ev_writept) c->ssl->cache_ev_writept = c->event->write_pt;

        if(sslerr == SSL_ERROR_WANT_READ) {
            event_opt(c->event, c->fd, EV_R);
        } else if(sslerr == SSL_ERROR_WANT_WRITE) {
            c->event->write_pt = ssl_write_handler;
            event_opt(c->event, c->fd, EV_W);
        }
        return -11;
    }
    ///if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
    ///    err("ssl read eof\n"); ///eof form ssl peer(shutdown)
    ///}
    ssl_dump_error(sslerr);
    return -1;
}

static int ssl_read_handler(event_t * ev)
{
    con_t * c = ev->data;
    if(c->ssl && c->ssl->cache_ev_readpt) {
        c->event->read_pt = c->ssl->cache_ev_readpt;
        c->ssl->cache_ev_readpt = NULL;
    }
    return ev->write_pt(ev);
}

int ssl_write(con_t * c, unsigned char * data, int datan)
{
    ssl_clear_error();
    
    int rc = SSL_write(c->ssl->con, data, datan);
    if(rc > 0) {
        if(c->ssl->cache_ev_typ) {
            event_opt(c->event, c->fd, c->ssl->cache_ev_typ);
            c->ssl->cache_ev_typ = 0;
        }
        if(c->ssl->cache_ev_readpt) {
            c->event->read_pt = c->ssl->cache_ev_readpt;
            c->ssl->cache_ev_readpt = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error(c->ssl->con, rc);
    if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if(!c->ssl->cache_ev_typ) c->ssl->cache_ev_typ = c->event->opt;
        if(!c->ssl->cache_ev_readpt) c->ssl->cache_ev_readpt = c->event->read_pt;
        if(!c->ssl->cache_ev_writept) c->ssl->cache_ev_writept = c->event->write_pt;

        if(sslerr == SSL_ERROR_WANT_READ) {
            c->event->read_pt = ssl_read_handler;
            event_opt(c->event, c->fd, EV_R);
        } else if(sslerr == SSL_ERROR_WANT_WRITE) {
            event_opt(c->event, c->fd, EV_W);
        }
        return -11;
    }
    ssl_dump_error(sslerr);
    return -1;
}

int ssl_write_chain(con_t * c, meta_t * meta)
{
    int sendn;
    meta_t * cl = meta;
    
    for(;;) {
        for(cl = meta; cl; cl = cl->next) {
            if(meta_len(cl->pos, cl->last)) {
                break;
            }
        }
        if(!cl)
            return 1;
        
        sendn = ssl_write(c, cl->pos, meta_len(cl->pos, cl->last));
        if(sendn < 0) {
            if(-11 == sendn) {
                return -11;
            }
            err("ssl write failed\n");
            return -1;
        }
        cl->pos += sendn;  
    }
}

int ssl_load_con_certificate(SSL_CTX * ctx, int flag, SSL ** ssl)
{
    SSL * local_ssl = NULL;
    
    local_ssl = SSL_new(ctx);
    if(!local_ssl) {
        err("ssl load con certificate, SSL_new failed\n");
        return -1;
    }
    
    if(flag == L_SSL_SERVER) {
        if(1 != SSL_use_certificate_file(local_ssl, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM)) {
            err("ssl load con certificate, SSL_use_certificate_file failed\n");
            SSL_free(local_ssl);
            return -1;
        }
        if(1 != SSL_use_PrivateKey_file(local_ssl, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM)) {
            err("ssl load con certificate, SSL_use_PrivateKey_file failed\n");
            SSL_free(local_ssl);
            return -1;
        }
        if(1 != SSL_check_private_key(local_ssl)) {
            err("ssl load con certificate, SSL_check_private_key failed\n");
            SSL_free(local_ssl);
            return -1;
        }
    }
    *ssl = local_ssl;
    return 0;
}

int ssl_load_ctx_certificate(SSL_CTX ** ctx, int flag)
{
    if(flag == L_SSL_CLIENT) {
        if(!g_ssl_ctx->ctx_client) {
            g_ssl_ctx->ctx_client = SSL_CTX_new(SSLv23_client_method());
            if(!g_ssl_ctx->ctx_client) {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return -1;
            }
        }
        *ctx = g_ssl_ctx->ctx_client;
    } else {
        if(!g_ssl_ctx->ctx_server) {
            g_ssl_ctx->ctx_server = SSL_CTX_new(SSLv23_server_method());
            if(!g_ssl_ctx->ctx_server) {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return -1;
            }
            if(1 != SSL_CTX_use_certificate_file(g_ssl_ctx->ctx_server, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM)) {
                err("ssl load ctx certificate, SSL_CTX_use_certificate_file failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return -1;
            }
            if(1 != SSL_CTX_use_PrivateKey_file(g_ssl_ctx->ctx_server, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM)) {
                err("ssl load ctx certificate, SSL_CTX_use_PrivateKey_file failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return -1;
            }
            if(1 != SSL_CTX_check_private_key(g_ssl_ctx->ctx_server)) {
                err("ssl load ctx certificate, SSL_CTX_check_private_key failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return -1;
            }
        }
        *ctx = g_ssl_ctx->ctx_server;
    }
    return 0;
}

int ssl_create_connection(con_t * c, int flag)
{
    ssl_con_t * sslcon = NULL;
    
    if ((flag != L_SSL_CLIENT) && (flag != L_SSL_SERVER)) {
        err("ssl create con flag not support\n");
        return -1;
    }
    
    do {
        sslcon = mem_pool_alloc(sizeof(ssl_con_t));
        if(!sslcon) {
            err("ssl create con alloc sslcon failed, [%d]\n", errno);
            return -1;
        }
        if(0 != ssl_load_ctx_certificate(&sslcon->session_ctx, flag)) {
            err("ssl create con, load ctx certificate failed\n");
            break;
        }
        if(0 != ssl_load_con_certificate(sslcon->session_ctx, flag, &sslcon->con)) {
            err("ssl create con, load con certificate failed\n");
            break;
        }
        if(0 == SSL_set_fd(sslcon->con, c->fd)) {
            err("ssl create, SSL_set_fd failed\n");
            break;
        }
        
        (flag == L_SSL_CLIENT) ? SSL_set_connect_state(sslcon->con) : SSL_set_accept_state(sslcon->con);       
        if(0 == SSL_set_ex_data(sslcon->con, 0, c)) {
            err("ssl create con set ex data failed\n");
            break;
        }
        
        c->ssl = sslcon;
        sslcon->data = c;
        return 0;
    } while(0);
    
    if(sslcon) {
        if(sslcon->con) {
            SSL_free(sslcon->con);
        }
        mem_pool_free(sslcon);
    }
    return -1;
}

int ssl_init(void)
{
    if(g_ssl_ctx) {
        err("ssl init this is not empty\n");
        return -1;
    }
    g_ssl_ctx = mem_pool_alloc(sizeof(g_ssl_t));
    if(!g_ssl_ctx) {
        err("ssl init alloc this failed, [%d]\n", errno);
        return -1;
    }

#if(1)
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#else
    OPENSSL_config(NULL);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
    return 0;
}

int ssl_end(void)
{
    ERR_free_strings();
    EVP_cleanup();
    if(g_ssl_ctx) {
        if(g_ssl_ctx->ctx_client) {
            SSL_CTX_free(g_ssl_ctx->ctx_client);
            g_ssl_ctx->ctx_client = NULL;
        }
        if(g_ssl_ctx->ctx_server) {
            SSL_CTX_free(g_ssl_ctx->ctx_server);
            g_ssl_ctx->ctx_server = NULL;
        }
        mem_pool_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
    return 0;
}

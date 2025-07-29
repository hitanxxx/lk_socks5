#include "common.h"


typedef struct {
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

int ssl_cexp(con_t * c)
{
    c->ssl->f_err = 1;
    return net_free(c);
}

int ssl_shutdown_cb(con_t * c)
{
    int rc = ssl_shutdown(c);
	
	if(rc == -1) {		///error
		tm_del(c);
		return net_free_direct(c);
	} else {
		if(rc == -11) {	///again
			tm_add(c, ssl_cexp, SSL_TMOUT);
			return -11;
		} 
		///success
		tm_del(c);
		return net_free_direct(c);
	}
}

int ssl_shutdown(con_t * c)
{
    ssl_con_t * sslc = c->ssl;
    int sslerr = 0;
    int t = 0;
    
    ssl_clear_error();

	if(SSL_in_init(sslc->con)) {
		c->ssl->f_closed = 1;
		return 0;
	}
	
    for(t = 0; t < 2; t++) {
        int rc = SSL_shutdown(sslc->con);
        if(rc == 1) {
            c->ssl->f_closed = 1;
            return 0;
        }
        if(rc == 0 && t == 0) {
            continue;
        }
        sslerr = SSL_get_error(sslc->con, rc);
        if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
            
            if(sslerr == SSL_ERROR_WANT_READ) {
                c->ev->read_cb = ssl_shutdown_cb;
				c->ev->write_cb = NULL;
                ev_opt(c, EV_R);
            } else {
				c->ev->read_cb = NULL;
                c->ev->write_cb = ssl_shutdown_cb;
                ev_opt(c, EV_W);
            }
            return -11;
        }
    }
    ssl_dump_error(sslerr);
    return -1;
}

static int ssl_handshake_cb(con_t * c)
{
    int rc = ssl_handshake(c);
	if(rc == -1) {
		tm_del(c);

		if(c->ssl->handshake_cb) {
			c->ev->write_cb = c->ssl->handshake_cb;
            ev_opt(c, EV_R);

			///give result to previous level
			return c->ev->write_cb(c);
		}
		return net_free_direct(c);
	} else {
		if(c->ssl->f_handshaked) {
			///success
			tm_del(c);

			///recovery cc opt and callback
			if(c->ssl->cc_ev_typ) 
				ev_opt(c, c->ssl->cc_ev_typ);

			///when in ssl handshake state. only have one kind of read/write
			if(c->ssl->cc_ev_cbw) {
				c->ev->write_cb = c->ssl->cc_ev_cbw;
				c->ssl->cc_ev_cbw = NULL;
			}

			if(c->ssl->cc_ev_cbr) {
				c->ev->read_cb = c->ssl->cc_ev_cbr;
				c->ssl->cc_ev_cbr = NULL;
			}

			if(c->ev->write_cb) 
				return c->ev->write_cb(c);

			return c->ev->read_cb(c);	
		}
		
		tm_add(c, ssl_cexp, SSL_TMOUT);
		return -11;
	}
}

int ssl_handshake(con_t * c)
{   
    ssl_con_t * sslc = c->ssl;
    ssl_clear_error();
	
    int rc = SSL_do_handshake(sslc->con);
    if(rc == 1) {
        sslc->f_handshaked = 1;
        return 0;
    }
    
    int sslerr = SSL_get_error(sslc->con, rc);
    if((sslerr == SSL_ERROR_WANT_READ) || (sslerr == SSL_ERROR_WANT_WRITE)) {
		
        if(!sslc->cc_ev_typ) 
            sslc->cc_ev_typ = c->ev->opt; ///cache current event opt
            
        if(!sslc->cc_ev_cbr)
            if(c->ev->read_cb) sslc->cc_ev_cbr = c->ev->read_cb;
			
        if(!sslc->cc_ev_cbw)
            if(c->ev->write_cb) sslc->cc_ev_cbw = c->ev->write_cb;
   
        if(sslerr == SSL_ERROR_WANT_READ) {
            c->ev->read_cb = ssl_handshake_cb;
            ev_opt(c, EV_R);
        } else if (sslerr == SSL_ERROR_WANT_WRITE) {
            c->ev->write_cb = ssl_handshake_cb;
            ev_opt(c, EV_W);
        }
        return -11;
    }
    ssl_dump_error(sslerr);
    return -1;
}

static int ssl_read_cb(con_t * c)
{
    return c->ev->read_cb(c);
}

int ssl_read(con_t * c, unsigned char * buf, int bufn)
{
    ssl_con_t * sslc = c->ssl;

    ssl_clear_error();
    int rc = SSL_read(sslc->con, buf, bufn);
    if(rc > 0) {
        if(sslc->cc_ev_typ) {
            ev_opt(c, sslc->cc_ev_typ);
               sslc->cc_ev_typ = 0;
        }
        if(sslc->cc_ev_cbw) {
            c->ev->write_cb = sslc->cc_ev_cbw;
            sslc->cc_ev_cbw = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error(sslc->con, rc);
    if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if(!sslc->cc_ev_typ) 
            sslc->cc_ev_typ = c->ev->opt;
        if(!sslc->cc_ev_cbr)
            if(c->ev->read_cb) sslc->cc_ev_cbr = c->ev->read_cb;
        if(!sslc->cc_ev_cbw)
            if(c->ev->write_cb) sslc->cc_ev_cbw = c->ev->write_cb;

        if(sslerr == SSL_ERROR_WANT_READ) {
            ev_opt(c, EV_R);
        } else if(sslerr == SSL_ERROR_WANT_WRITE) {
            c->ev->write_cb = ssl_read_cb;
            ev_opt(c, EV_W);
        }
        return -11;
    }
    ///if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
    ///    err("ssl read eof\n"); ///eof form ssl peer(shutdown)
    ///}
    ssl_dump_error(sslerr);
    return -1;
}

static int ssl_write_cb(con_t * c)
{
    return c->ev->write_cb(c);
}

int ssl_write(con_t * c, unsigned char * data, int datan)
{
    ssl_con_t * sslc = c->ssl;

    ssl_clear_error();
    int rc = SSL_write(sslc->con, data, datan);
    if(rc > 0) {
        if(sslc->cc_ev_typ) {
            ev_opt(c, c->ssl->cc_ev_typ);
            sslc->cc_ev_typ = 0;
        }
        if(sslc->cc_ev_cbr) {
            c->ev->read_cb = sslc->cc_ev_cbr;
            sslc->cc_ev_cbr = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error(sslc->con, rc);
    if(sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        if(!sslc->cc_ev_typ)
            c->ssl->cc_ev_typ = c->ev->opt;
        if(!sslc->cc_ev_cbr) 
            if(c->ev->read_cb) c->ssl->cc_ev_cbr = c->ev->read_cb;
        if(!sslc->cc_ev_cbw)
            if(c->ev->write_cb) c->ssl->cc_ev_cbw = c->ev->write_cb;

        if(sslerr == SSL_ERROR_WANT_READ) {
            c->ev->read_cb = ssl_write_cb;
            ev_opt(c, EV_R);
        } else if(sslerr == SSL_ERROR_WANT_WRITE) {
            ev_opt(c, EV_W);
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
            if(meta_getlen(cl)) {
                break;
            }
        }
        if(!cl)
            return 1;
        
        sendn = ssl_write(c, cl->pos, meta_getlen(cl));
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
    schk(local_ssl = SSL_new(ctx), return -1);
    
    if(flag == L_SSL_SERVER) {
        int ret = -1;
        do {
            schk(SSL_use_certificate_file(local_ssl, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM) == 1, break);
            schk(SSL_use_PrivateKey_file(local_ssl, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM) == 1, break);
            schk(SSL_check_private_key(local_ssl) == 1, break);
            ret = 0;
        } while(0);
        if(ret != 0) {
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
            schk(g_ssl_ctx->ctx_client = SSL_CTX_new(TLS_client_method()), return -1);
        }
        *ctx = g_ssl_ctx->ctx_client;
    } else {
        if(!g_ssl_ctx->ctx_server) {
            int ret = -1;
            do {
                schk(g_ssl_ctx->ctx_server = SSL_CTX_new(TLS_server_method()), return -1);
                schk(SSL_CTX_use_certificate_file(g_ssl_ctx->ctx_server, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM) == 1, break);
                schk(SSL_CTX_use_PrivateKey_file(g_ssl_ctx->ctx_server, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM) == 1, break);
                schk(SSL_CTX_check_private_key(g_ssl_ctx->ctx_server) == 1, break);
                ret = 0;
            } while(0);

            if(ret != 0) {
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
    ssl_con_t * sslc = NULL;
    schk(((flag == L_SSL_SERVER) || (flag == L_SSL_CLIENT)), return -1);
    
    do {
        sslc = mem_pool_alloc(sizeof(ssl_con_t));
        schk(sslc, return -1);
        schk(ssl_load_ctx_certificate(&sslc->session_ctx, flag) == 0, break);
        schk(ssl_load_con_certificate(sslc->session_ctx, flag, &sslc->con) == 0, break);
        schk(SSL_set_fd(sslc->con, c->fd) != 0, break);
        (flag == L_SSL_CLIENT) ? SSL_set_connect_state(sslc->con) : SSL_set_accept_state(sslc->con);
        schk(SSL_set_ex_data(sslc->con, 0, c) != 0, break);
        
        c->ssl = sslc;
        sslc->data = c;
        return 0;
    } while(0);
    
    if(sslc) {
        if(sslc->con) {
            SSL_free(sslc->con);
        }
        mem_pool_free(sslc);
    }
    return -1;
}

int ssl_init(void)
{
    schk(!g_ssl_ctx, return -1);
    schk(g_ssl_ctx = mem_pool_alloc(sizeof(g_ssl_t)), return -1);

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
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

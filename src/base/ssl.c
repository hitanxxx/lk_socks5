#include "common.h"



typedef struct
{
    SSL_CTX *       ctx_client;
    SSL_CTX *       ctx_server;
    char            g_err_msg[1024];
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
    if( ERR_peek_error() ) { \
        while( (n = ERR_peek_error()) ) { \
            if( p < last - 1 ) { \
                *p++ = ' '; \
                ERR_error_string_n( n, (char*)p, last-p ); \
            } \
            (void)ERR_get_error(); \
        } \
        err("ssl err. [%s]\n", errstr ); \
    } \
}


status ssl_shutdown_handler( event_t * ev )
{
    con_t * c = ev->data;
    ssl_layer_done_pt cb = c->ssl->cb;
    timer_del( &ev->timer );

    int rc = ssl_shutdown(c->ssl);
    if(rc==AGAIN) {
        timer_add( &ev->timer, L_SSL_TIMEOUT );
        return AGAIN;
    }
    
    if( c->ssl->cache_ev_typ ) {
        event_opt( ev, c->fd, c->ssl->cache_ev_typ );
        c->ssl->cache_ev_typ = 0;
    }
    return (cb?(cb(c->event)):rc);
}

status ssl_shutdown( ssl_con_t * ssl )
{
    con_t * c = ssl->data;
    int sslerr = 0;
    
    ssl_clear_error();

    int try = 0;
    for( try = 0; try < 2; try ++ ) {
        int rc = SSL_shutdown( c->ssl->con );
        if(rc==1) {
            return OK;
        }
        if(rc == 0 && try == 0) {
            continue;
        }

        sslerr = SSL_get_error(c->ssl->con, rc );
        if(sslerr == SSL_ERROR_WANT_READ||sslerr == SSL_ERROR_WANT_WRITE) {
            c->ssl->f_shutdown_send = 1;
            
            if(0 == ssl->cache_ev_typ) {
                ssl->cache_ev_typ = c->event->opt;
            }
            if(sslerr == SSL_ERROR_WANT_READ) {
                c->event->read_pt = ssl_shutdown_handler;
                event_opt(c->event, c->fd, EV_R);
            } else {
                c->event->write_pt = ssl_shutdown_handler;
                event_opt(c->event, c->fd, EV_W);
            }
            return AGAIN;
        }
    }
    ssl_dump_error(sslerr);
    
    SSL_free(c->ssl->con);
    mem_pool_free(c->ssl);
    c->ssl = NULL;
    c->fssl = 0;
    return ERROR;
}


static status ssl_handshake_handler( event_t * ev )
{
    con_t * c = ev->data;

    timer_del( &ev->timer );
    int rc = ssl_handshake(c->ssl);
    if(rc<0) {
        if(rc == AGAIN) {
            timer_add( &ev->timer, L_SSL_TIMEOUT );
            return AGAIN;
        }
        assert(c->event->timer.timeout_handler);
        assert(c->event->timer.data);
        c->event->timer.timeout_handler(c->event->timer.data);
        return ERROR;
    }
    
    if( c->ssl->cache_ev_typ ) {
        event_opt( ev, c->fd, c->ssl->cache_ev_typ );
        c->ssl->cache_ev_typ = 0;
    }
    assert(c->ssl->cb);
    return c->ssl->cb( ev );
}

status ssl_handshake( ssl_con_t * ssl )
{
    con_t * c = ssl->data;
    
    ssl_clear_error(); ///clear ssl error stack 
    int rc = SSL_do_handshake( ssl->con );
    if( rc == 1 ) {
        ssl->f_handshaked = 1;
        return OK;
    }
    
    int sslerr = SSL_get_error( ssl->con, rc );
    if( sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE ) {

        if( 0 == ssl->cache_ev_typ ) {
            ssl->cache_ev_typ = c->event->opt; ///cache current event opt
        }
        if( sslerr == SSL_ERROR_WANT_READ ) {
            c->event->read_pt = ssl_handshake_handler;
            event_opt(c->event, c->fd, EV_R);
        } else if ( sslerr == SSL_ERROR_WANT_WRITE ) {
            c->event->write_pt = ssl_handshake_handler;
            event_opt(c->event, c->fd, EV_W);
        }
        return AGAIN;
    }
    ssl_dump_error(sslerr);

    SSL_free(c->ssl->con);
    mem_pool_free(c->ssl);
    c->ssl = NULL;
    c->fssl = 0;
    return ERROR;
}

inline static status ssl_write_handler( event_t * ev)
{
    con_t * c = ev->data;
    if(c->ssl->cache_ev_writept) {
        c->event->write_pt = c->ssl->cache_ev_writept;
        c->ssl->cache_ev_writept = NULL;
    }
    return ev->read_pt( ev );
}

ssize_t ssl_read( con_t * c, unsigned char * start, uint32 len )
{
    ssl_clear_error();
    int rc = SSL_read( c->ssl->con, start, (int)len );
    if( rc > 0 ) {
        if(c->ssl->cache_ev_typ) {
            event_opt( c->event, c->fd, c->ssl->cache_ev_typ );
            c->ssl->cache_ev_typ = 0;
        }
        if(c->ssl->cache_ev_writept) {
            c->event->write_pt = c->ssl->cache_ev_writept;
            c->ssl->cache_ev_writept = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error( c->ssl->con, rc );
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
        return AGAIN;
    } else if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        ///err("ssl read eof\n"); /// not clear ssl data, will be call shutdown again
        ssl_dump_error(sslerr);
        return ERROR; 
    }
    ssl_dump_error(sslerr);

    SSL_free(c->ssl->con);
    mem_pool_free(c->ssl);
    c->ssl = NULL;
    c->fssl = 0;
    return ERROR;
}

static status ssl_read_handler( event_t * ev )
{
    con_t * c = ev->data;
    if(c->ssl->cache_ev_readpt) {
        c->event->read_pt = c->ssl->cache_ev_readpt;
        c->ssl->cache_ev_readpt = NULL;
    }
    return ev->write_pt( ev );
}

ssize_t ssl_write( con_t * c, unsigned char * start, uint32 len )
{
    ssl_clear_error();
    
    int rc = SSL_write( c->ssl->con, start, (int)len );
    if( rc > 0 ) {
        if(c->ssl->cache_ev_typ) {
            event_opt( c->event, c->fd, c->ssl->cache_ev_typ );
            c->ssl->cache_ev_typ = 0;
        }
        if(c->ssl->cache_ev_readpt) {
            c->event->read_pt = c->ssl->cache_ev_readpt;
            c->ssl->cache_ev_readpt = NULL;
        }
        return rc;
    }
    
    int sslerr = SSL_get_error( c->ssl->con, rc );
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
        return AGAIN;
    }
    ssl_dump_error(sslerr);

    SSL_free(c->ssl->con);
    mem_pool_free(c->ssl);
    c->ssl = NULL;
    c->fssl = 0;
    return ERROR;
}

status ssl_write_chain( con_t * c, meta_t * meta )
{
    ssize_t sent;
    meta_t * cl = meta;
    
    while(1) {
        for( cl = meta; cl; cl = cl->next ) {
            if( meta_len( cl->pos, cl->last ) ) {
                break;
            }
        }
        if( !cl ) {
            return DONE;
        }
        
        sent = ssl_write( c, cl->pos, meta_len( cl->pos, cl->last) );
        if( ERROR == sent ) {
            err ("ssl write failed\n");
            return ERROR;
        } else if ( AGAIN == sent ) {
            return AGAIN;
        } else {
            cl->pos += sent;
        }
    }
}

status ssl_load_con_certificate( SSL_CTX * ctx, int flag, SSL ** ssl )
{
    SSL * local_ssl = NULL;
    
    local_ssl = SSL_new( ctx );
    if( !local_ssl ) {
        err("ssl load con certificate, SSL_new failed\n");
        return ERROR;
    }
    
    if( flag == L_SSL_SERVER ) {
        if( 1 != SSL_use_certificate_file( local_ssl, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM ) ) {
            err("ssl load con certificate, SSL_use_certificate_file failed\n");
            SSL_free( local_ssl );
            return ERROR;
        }
        if( 1 != SSL_use_PrivateKey_file( local_ssl, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM ) ) {
            err("ssl load con certificate, SSL_use_PrivateKey_file failed\n");
            SSL_free( local_ssl );
            return ERROR;
        }
        if( 1 != SSL_check_private_key( local_ssl ) ) {
            err("ssl load con certificate, SSL_check_private_key failed\n");
            SSL_free( local_ssl );
            return ERROR;
        }
    }
    *ssl = local_ssl;
    return OK;
}

status ssl_load_ctx_certificate( SSL_CTX ** ctx, int flag )
{
    if( flag == L_SSL_CLIENT ) {
        if( !g_ssl_ctx->ctx_client ) {
            g_ssl_ctx->ctx_client = SSL_CTX_new( SSLv23_client_method( ));
            if( !g_ssl_ctx->ctx_client ) {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return ERROR;
            }
        }
        *ctx = g_ssl_ctx->ctx_client;
    } else {
        if( !g_ssl_ctx->ctx_server ) {
            g_ssl_ctx->ctx_server = SSL_CTX_new( SSLv23_server_method( ));
            if( !g_ssl_ctx->ctx_server ) {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return ERROR;
            }
            if( 1 != SSL_CTX_use_certificate_file( g_ssl_ctx->ctx_server, (char*)config_get()->ssl_crt_path, SSL_FILETYPE_PEM 
            ) ) {
                err("ssl load ctx certificate, SSL_CTX_use_certificate_file failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return ERROR;
            }
            if( 1 != SSL_CTX_use_PrivateKey_file( g_ssl_ctx->ctx_server, (char*)config_get()->ssl_key_path, SSL_FILETYPE_PEM 
            ) ) {
                err("ssl load ctx certificate, SSL_CTX_use_PrivateKey_file failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return ERROR;
            }
            if( 1 != SSL_CTX_check_private_key( g_ssl_ctx->ctx_server ) ) {
                err("ssl load ctx certificate, SSL_CTX_check_private_key failed\n");
                SSL_CTX_free(g_ssl_ctx->ctx_server);
                g_ssl_ctx->ctx_server = NULL;
                return ERROR;
            }
        }
        *ctx = g_ssl_ctx->ctx_server;
    }
    return OK;
}

status ssl_create_connection( con_t * c, uint32 flag )
{
    ssl_con_t * sslcon = NULL;
    
    if ( (flag != L_SSL_CLIENT) && (flag != L_SSL_SERVER) ) {
        err("ssl create con flag not support\n");
        return ERROR;
    }
    
    do {
        sslcon = mem_pool_alloc( sizeof(ssl_con_t) );
        if( !sslcon ) {
            err("ssl create con alloc sslcon failed, [%d]\n", errno );
            return ERROR;
        }
        
        if( OK != ssl_load_ctx_certificate( &sslcon->session_ctx, flag ) ) {
            err("ssl create con, load ctx certificate failed\n");
            break;
        }
        
        if( OK != ssl_load_con_certificate(sslcon->session_ctx, flag, &sslcon->con ) ) {
            err("ssl create con, load con certificate failed\n");
            break;
        }
        
        if( 0 == SSL_set_fd( sslcon->con, c->fd ) ) {
            err("ssl create, SSL_set_fd failed\n");
            break;
        }
        
        ( flag == L_SSL_CLIENT ) ? SSL_set_connect_state( sslcon->con ) : SSL_set_accept_state( sslcon->con );
        
        if( 0 == SSL_set_ex_data( sslcon->con, 0, c ) ) {
            err("ssl create con set ex data failed\n");
            break;
        }
        
        c->ssl = sslcon;
        sslcon->data = c;
        return OK;
    } while(0);
    
    if( sslcon ) {
        if( sslcon->con ) {
            SSL_free( sslcon->con );
        }
        mem_pool_free( sslcon );
    }
    return ERROR;
}

status ssl_init( void )
{
    if( g_ssl_ctx != NULL ) {
        err("ssl init this is not empty\n");
        return ERROR;
    }
    g_ssl_ctx = mem_pool_alloc(sizeof(g_ssl_t));
    if( !g_ssl_ctx ) {
        err("ssl init alloc this failed, [%d]\n", errno );
        return ERROR;
    }

#if(1)
    SSL_library_init( );
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#else
    OPENSSL_config(NULL);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
    return OK;
}

status ssl_end( void )
{
    ERR_free_strings();
    EVP_cleanup();
    if( g_ssl_ctx ) {
        if( g_ssl_ctx->ctx_client ) {
            SSL_CTX_free( g_ssl_ctx->ctx_client );
            g_ssl_ctx->ctx_client = NULL;
        }
        if( g_ssl_ctx->ctx_server ) {
            SSL_CTX_free( g_ssl_ctx->ctx_server );
            g_ssl_ctx->ctx_server = NULL;
        }
        mem_pool_free( g_ssl_ctx );
        g_ssl_ctx = NULL;
    }
    return OK;
}

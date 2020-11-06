#include "l_base.h"

typedef struct private_ssl
{
    SSL_CTX *       ctx_client;
    SSL_CTX *       ctx_server;
    char            g_err_msg[1024];
} private_ssl_t;
static private_ssl_t * this = NULL;

static void ssl_clear_error( void )
{
    // ignoring stale global SSL error
    unsigned long rc = 0;
    do
    {
        rc = ERR_peek_error();
        err("ssl clear error, ignore stale error [%d]\n", rc );
    }while(rc);
    ERR_clear_error();
}

static void ssl_record_error( int sslerr )
{
    unsigned long n = 0;
    unsigned char errstr[512] = {0};
    unsigned char *p = errstr, *last = errstr + sizeof(errstr);
    
    if( sslerr == SSL_ERROR_SSL )
    {
        if( ERR_peek_error() )
        {
            while( (n = ERR_peek_error()) )
            {
                if( p < last - 1 )
                {
                    *p++ = ' ';
                    ERR_error_string_n( n, (char*)p, last-p );
                }
                (void)ERR_get_error();
            }
        }
        err("ssl record error, [%s]\n", errstr );
    }
}

status ssl_shutdown_handler( event_t * ev )
{
    connection_t * c = ev->data;
    ssl_handshake_pt  callback = c->ssl->cb;
    
    if( ssl_shutdown( c->ssl ) == AGAIN )
    {
        return AGAIN;
    }
    if( c->ssl->cache_ev_type )
    {
        event_opt( ev, c->fd, c->ssl->cache_ev_type );
        c->ssl->cache_ev_type = 0;
    }
    return callback( c->event );
}

status ssl_shutdown( ssl_connection_t * ssl )
{
    int32 n, sslerr = 0;
    connection_t * c = ssl->data;
    
    if( SSL_in_init( c->ssl->con ) )
    {
        SSL_free( c->ssl->con );
        l_safe_free( c->ssl );
        c->ssl      = NULL;
        c->ssl_flag = 0;
        return OK;
    }
    ssl_clear_error();
    n = SSL_shutdown( c->ssl->con );
    if( n != 1 && ERR_peek_error() )
    {
        sslerr = SSL_get_error(c->ssl->con, n );
    }
    if( n == 1 || sslerr == 0 || sslerr == SSL_ERROR_ZERO_RETURN )
    {
        SSL_free( c->ssl->con );
        l_safe_free( c->ssl );
        c->ssl      = NULL;
        c->ssl_flag = 0;
        return OK;
    }
    
    if( sslerr == SSL_ERROR_WANT_READ )
    {
        if( 0 == ssl->cache_ev_type )
        {
            ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_R );
        c->event->read_pt	= ssl_shutdown_handler;
        return AGAIN;
    }
    else if ( sslerr == SSL_ERROR_WANT_WRITE )
    {
        if( 0 == ssl->cache_ev_type )
        {
            ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_W );
        c->event->write_pt	= ssl_shutdown_handler;
        return AGAIN;
    }
    
    SSL_free( c->ssl->con );
    l_safe_free( c->ssl );
    c->ssl      = NULL;
    c->ssl_flag = 0;
    ssl_record_error(sslerr);
    return ERROR;
}

static status ssl_handshake_handler( event_t * ev )
{
    connection_t * c = ev->data;
    
    if( ssl_handshake( c->ssl ) == AGAIN )
    {
        return AGAIN;
    }
    if( c->ssl->cache_ev_type )
    {
        event_opt( ev, c->fd, c->ssl->cache_ev_type );
        c->ssl->cache_ev_type = 0;
    }
    return c->ssl->cb( ev );
}

status ssl_handshake( ssl_connection_t * ssl )
{
    int n, sslerr;
    connection_t * c = ssl->data;
    
    ssl_clear_error();
    n = SSL_do_handshake( ssl->con );
    if( n == 1 )
    {
        ssl->handshaked = 1;
        return OK;
    }
    sslerr = SSL_get_error( ssl->con, n );
    if( sslerr == SSL_ERROR_WANT_READ )
    {
        if( 0 == ssl->cache_ev_type )
        {
            ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_R );
        c->event->read_pt = ssl_handshake_handler;
        return AGAIN;
    }
    else if ( sslerr == SSL_ERROR_WANT_WRITE )
    {
        if( 0 == ssl->cache_ev_type )
        {
            ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_W );
        c->event->write_pt = ssl_handshake_handler;
        return AGAIN;
    }
    
    if( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 )
    {
        err ("ssl handshake peer closed\n" );
        return ERROR;
    }
    ssl_record_error(sslerr);
    return ERROR;
}

inline static status ssl_write_handler( event_t * ev)
{
    return ev->read_pt( ev );
}

ssize_t ssl_read( connection_t * c, unsigned char * start, uint32 len )
{
    int rc = 0, sslerr = 0;
    
    ssl_clear_error();
    rc = SSL_read( c->ssl->con, start, (int)len );
    if( rc > 0 )
    {
        if( c->ssl->cache_ev_type )
        {
            event_opt( c->event, c->fd, c->ssl->cache_ev_type );
            c->ssl->cache_ev_type = 0;
        }
        if( c->ssl->cache_ev_handler )
        {
            c->event->write_pt = c->ssl->cache_ev_handler;
            c->ssl->cache_ev_handler = NULL;
        }
        return rc;
    }
    sslerr = SSL_get_error( c->ssl->con, rc );
    if( sslerr == SSL_ERROR_WANT_READ )
    {
        if( c->ssl->cache_ev_type == 0 )
        {
            c->ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_R );
        return AGAIN;
    }
    else if( sslerr == SSL_ERROR_WANT_WRITE )
    {
        // ssl read mabey re-negotiation, need to set write handler to ssl read
        if( c->ssl->cache_ev_type == 0 )
        {
            c->ssl->cache_ev_type = c->event->type;
        }
        if( c->ssl->cache_ev_handler == NULL )
        {
            c->ssl->cache_ev_handler = c->event->write_pt;
        }
        event_opt( c->event, c->fd, EV_W );
        c->event->write_pt = ssl_write_handler;
        return AGAIN;
    }
    else if( sslerr == SSL_ERROR_SYSCALL )
    {
        err("ssl error, syserror [%d]\n", errno );
    }
    else
    {
        err("ssl error, [%s]\n", ERR_error_string( ERR_get_error(), this->g_err_msg ));
    }
    if ( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 )
    {
        return ERROR;
    }
    ssl_record_error(sslerr);
    return ERROR;
}

static status ssl_read_handler( event_t * ev )
{
    return ev->write_pt( ev );
}

ssize_t ssl_write( connection_t * c, unsigned char * start, uint32 len )
{
    int rc = 0, sslerr = 0;
    
    ssl_clear_error();
    rc = SSL_write( c->ssl->con, start, (int)len );
    if( rc > 0 )
    {
        if( c->ssl->cache_ev_type )
        {
            event_opt( c->event, c->fd, c->ssl->cache_ev_type );
            c->ssl->cache_ev_type = 0;
        }
        if( c->ssl->cache_ev_handler )
        {
            c->event->read_pt = c->ssl->cache_ev_handler;
            c->ssl->cache_ev_handler = NULL;
        }
        return rc;
    }
    sslerr = SSL_get_error( c->ssl->con, rc );
    if( sslerr == SSL_ERROR_WANT_WRITE )
    {
        if( c->ssl->cache_ev_type == 0 )
        {
            c->ssl->cache_ev_type = c->event->type;
        }
        event_opt( c->event, c->fd, EV_W );
        return AGAIN;
    }
    else if( sslerr == SSL_ERROR_WANT_READ  )
    {
        // ssl write mabey re-negotiation, need to set read handler to ssl write
        if( c->ssl->cache_ev_type == 0 )
        {
            c->ssl->cache_ev_type = c->event->type;
        }
        if( c->ssl->cache_ev_handler == NULL )
        {
            c->ssl->cache_ev_handler = c->event->read_pt;
        }
        event_opt( c->event, c->fd, EV_R );
        c->event->write_pt = ssl_read_handler;
        return AGAIN;
    }
    else if( sslerr == SSL_ERROR_SYSCALL )
    {
        err("ssl error, syserror [%d]\n", errno );
    }
    else
    {
        err("ssl error, [%s]\n", ERR_error_string( ERR_get_error(), this->g_err_msg ));
    }
    if ( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 )
    {
        return ERROR;
    }
    ssl_record_error(sslerr);
    return ERROR;
}

status ssl_write_chain( connection_t * c, meta_t * meta )
{
    ssize_t sent;
    meta_t * cl = meta;
    
    while(1)
    {
        for( cl = meta; cl; cl = cl->next )
        {
            if( meta_len( cl->pos, cl->last ) )
            {
                break;
            }
        }
        if( !cl )
        {
            return DONE;
        }
        sent = ssl_write( c, cl->pos, meta_len( cl->pos, cl->last) );
        if( ERROR == sent )
        {
            err ("ssl write failed\n");
            return ERROR;
        }
        else if ( AGAIN == sent )
        {
            return AGAIN;
        }
        else
        {
            cl->pos += sent;
        }
    }
}

status ssl_load_con_certificate( SSL_CTX * ctx, int flag, SSL ** ssl )
{
    SSL * local_ssl = NULL;
    
    local_ssl = SSL_new( ctx );
    if( !local_ssl )
    {
        err("ssl load con certificate, SSL_new failed\n");
        return ERROR;
    }
    
    if( flag == L_SSL_SERVER )
    {
        if( 1 != SSL_use_certificate_file( local_ssl, (char*)conf.base.sslcrt, SSL_FILETYPE_PEM ) )
        {
            err("ssl load con certificate, SSL_use_certificate_file failed\n");
            SSL_free( local_ssl );
            return ERROR;
        }
        if( 1 != SSL_use_PrivateKey_file( local_ssl, (char*)conf.base.sslkey, SSL_FILETYPE_PEM ) )
        {
            err("ssl load con certificate, SSL_use_PrivateKey_file failed\n");
            SSL_free( local_ssl );
            return ERROR;
        }
        if( 1 != SSL_check_private_key( local_ssl ) )
        {
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
    if( flag == L_SSL_CLIENT )
    {
        if( !this->ctx_client )
        {
            this->ctx_client = SSL_CTX_new( SSLv23_client_method( ));
            if( !this->ctx_client )
            {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return ERROR;
            }
        }
        *ctx = this->ctx_client;
    }
    else
    {
        if( !this->ctx_server )
        {
            this->ctx_server = SSL_CTX_new( SSLv23_server_method( ));
            if( !this->ctx_server )
            {
                err("ssl load ctx certificate, SSL_CTX_new failed\n");
                return ERROR;
            }
            if( 1 != SSL_CTX_use_certificate_file( this->ctx_server, (char*)conf.base.sslcrt, SSL_FILETYPE_PEM ) )
            {
                err("ssl load ctx certificate, SSL_CTX_use_certificate_file failed\n");
                SSL_CTX_free(this->ctx_server);
                this->ctx_server = NULL;
                return ERROR;
            }
            if( 1 != SSL_CTX_use_PrivateKey_file( this->ctx_server, (char*)conf.base.sslkey, SSL_FILETYPE_PEM ) )
            {
                err("ssl load ctx certificate, SSL_CTX_use_PrivateKey_file failed\n");
                SSL_CTX_free(this->ctx_server);
                this->ctx_server = NULL;
                return ERROR;
            }
            if( 1 != SSL_CTX_check_private_key( this->ctx_server ) )
            {
                err("ssl load ctx certificate, SSL_CTX_check_private_key failed\n");
                SSL_CTX_free(this->ctx_server);
                this->ctx_server = NULL;
                return ERROR;
            }
        }
        *ctx = this->ctx_server;
    }
    return OK;
}

status ssl_create_connection( connection_t * c, uint32 flag )
{
    ssl_connection_t * sslcon = NULL;
    
    if ( (flag != L_SSL_CLIENT) && (flag != L_SSL_SERVER) )
    {
        err("ssl create con flag not support\n");
        return ERROR;
    }
    
    do
    {
        sslcon = l_safe_malloc( sizeof(ssl_connection_t) );
        if( !sslcon )
        {
            err("ssl create con alloc sslcon failed, [%d]\n", errno );
            return ERROR;
        }
        memset( sslcon, 0, sizeof(ssl_connection_t) );
        
        if( OK != ssl_load_ctx_certificate( &sslcon->session_ctx, flag ) )
        {
            err("ssl create con, load ctx certificate failed\n");
            break;
        }
        
        if( OK != ssl_load_con_certificate(sslcon->session_ctx, flag, &sslcon->con ) )
        {
            err("ssl create con, load con certificate failed\n");
            break;
        }
        
        if( 0 == SSL_set_fd( sslcon->con, c->fd ) )
        {
            err("ssl create, SSL_set_fd failed\n");
            break;
        }
        
        ( flag == L_SSL_CLIENT ) ? SSL_set_connect_state( sslcon->con ) : SSL_set_accept_state( sslcon->con );
        
        if( 0 == SSL_set_ex_data( sslcon->con, 0, c ) )
        {
            err("ssl create con set ex data failed\n");
            break;
        }
        
        c->ssl          = sslcon;
        sslcon->data    = c;
        return OK;
    } while(0);
    
    if( sslcon )
    {
        if( sslcon->con )
        {
            SSL_free( sslcon->con );
        }
        l_safe_free( sslcon );
    }
    return ERROR;
}

status ssl_init( void )
{
    if( this )
    {
        err("ssl init this is not empty\n");
        return ERROR;
    }
    this = l_safe_malloc(sizeof(private_ssl_t));
    if( !this )
    {
        err("ssl init alloc this failed, [%d]\n", errno );
        return ERROR;
    }
    memset( this, 0, sizeof(private_ssl_t) );
    
    SSL_library_init( );
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    return OK;
}

status ssl_end( void )
{
    ERR_free_strings();
    EVP_cleanup();
    return OK;
}

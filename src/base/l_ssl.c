#include "l_base.h"

static SSL_CTX * ctx_client = NULL;
static SSL_CTX * ctx_server = NULL;
static status ssl_write_handler( event_t * event );
static status ssl_read_handler( event_t * event );
static char g_err_msg[1024] = {0};


status ssl_shutdown_handler( event_t * ev )
{
	connection_t * c = ev->data;
	ssl_handshake_pt  callback = c->ssl->cb;

	if( ssl_shutdown( c->ssl ) == AGAIN ) 
	{
		return AGAIN;
	}
	return callback( &c->event );
}

status ssl_shutdown( ssl_connection_t * ssl )
{
	int32 n, ssler = 0;
	connection_t * c = ssl->data;

	if( SSL_in_init( c->ssl->con ) ) 
	{
		SSL_free( c->ssl->con );
		l_safe_free( c->ssl );
		c->ssl = NULL;
		c->ssl_flag = 0;
		return OK;
	}
	
	n = SSL_shutdown( c->ssl->con );
	if( n != 1 && ERR_peek_error() ) 
	{
		ssler = SSL_get_error(c->ssl->con, n );
	}
	if( n == 1 || ssler == 0 || ssler == SSL_ERROR_ZERO_RETURN ) 
	{
		SSL_free( c->ssl->con );
		l_safe_free( c->ssl );
		c->ssl = NULL;
		c->ssl_flag = 0;
		return OK;
	}

	if( ssler == SSL_ERROR_WANT_READ || ssler == SSL_ERROR_WANT_WRITE )
	{
		event_opt( &c->event, c->fd, EV_W );
		event_opt( &c->event, c->fd, EV_R );
		c->event.write_pt 	= ssl_shutdown_handler;
		c->event.read_pt 	= ssl_shutdown_handler;
		return AGAIN;
	}

	SSL_free( c->ssl->con );
	l_safe_free( c->ssl );
	c->ssl = NULL;
	c->ssl_flag = 0;
	return ERROR;
}

static status ssl_handshake_handler( event_t * ev )
{
	connection_t * c = ev->data;
	
	if( ssl_handshake( c->ssl ) == AGAIN ) 
	{
		return AGAIN;
	}
	return c->ssl->cb( ev );
}

status ssl_handshake( ssl_connection_t * ssl )
{
	int n, ssler;
	connection_t * c = ssl->data;
	
	n = SSL_do_handshake( ssl->con );
	if( n == 1 ) 
	{
		ssl->handshaked = 1;
		return OK;
	}
	ssler = SSL_get_error( ssl->con, n );
	if( ssler == SSL_ERROR_WANT_READ || ssler == SSL_ERROR_WANT_WRITE )
	{
		event_opt( &c->event, c->fd, EV_W );
		event_opt( &c->event, c->fd, EV_R );
		c->event.write_pt 	= ssl_handshake_handler;
		c->event.read_pt 	= ssl_handshake_handler;
		return AGAIN;
	}

	if( ssler == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 ) 
	{
		err ( " peer closed\n" );
		return ERROR;
	}
	return ERROR;
}


ssize_t ssl_read( connection_t * c, char * start, uint32 len )
{
	int rc, sslerr;

	rc = SSL_read( c->ssl->con, start, (int)len );
	if( rc > 0 )
	{
		return rc;
	}
	sslerr = SSL_get_error( c->ssl->con, rc );
	if( sslerr == SSL_ERROR_WANT_READ ) 
	{
		return AGAIN;
	}
	else if( sslerr == SSL_ERROR_WANT_WRITE ) 
	{
		return AGAIN;
	}
	else if( sslerr == SSL_ERROR_SYSCALL ) 
	{
		err("ssl error, syserror [%d]\n", errno );
	} 
	else 
	{
		err("ssl error, [%s]\n", ERR_error_string(ERR_get_error(),g_err_msg ));
	}
	if ( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 ) 
	{
		return ERROR;
	}
	return ERROR;
}

static status ssl_write_handler( event_t * ev )
{
	connection_t * c = ev->data;
	return c->event.read_pt( &c->event );
}

ssize_t ssl_write( connection_t * c, char * start, uint32 len )
{
	int rc, sslerr;

	rc = SSL_write( c->ssl->con, start, (int)len );
	if( rc > 0 ) 
	{
		return rc;
	}
	sslerr = SSL_get_error( c->ssl->con, rc );
	if( sslerr == SSL_ERROR_WANT_WRITE ) 
	{
		return AGAIN;
	}
	else if( sslerr == SSL_ERROR_WANT_READ )
	{
		return AGAIN;
	}
	else if( sslerr == SSL_ERROR_SYSCALL ) 
	{
		err("ssl error, syserror [%d]\n", errno );
	}
	else
	{
		err("ssl error, [%s]\n", ERR_error_string(ERR_get_error(),g_err_msg ));
	}
	if ( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 ) 
	{
		return ERROR;
	}
	return ERROR;
}

static status ssl_read_handler( event_t * ev )
{
	connection_t * c = ev->data;
	return c->event.write_pt( &c->event );
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
			err ("ssl write\n");
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

status ssl_client_ctx( SSL_CTX ** s )
{
	if( ctx_client == NULL ) 
	{
		ctx_client = SSL_CTX_new( SSLv23_client_method() );
	}
	*s = ctx_client;
	return OK;
}

status ssl_server_ctx( SSL_CTX ** s )
{
	int32 rc;
	SSL_CTX * local_ctx = NULL;

	if( !ctx_server ) 
	{
		local_ctx = SSL_CTX_new( SSLv23_server_method() );
		rc = SSL_CTX_use_certificate_chain_file (local_ctx, conf.base.sslcrt);
		if( rc != 1 ) 
		{
			err ("crt file [%s], failed. error [%s]\n", conf.base.sslcrt, ERR_error_string(ERR_get_error(),g_err_msg) );
			return ERROR;
		}
		rc = SSL_CTX_use_PrivateKey_file( local_ctx, conf.base.sslkey, SSL_FILETYPE_PEM );
		if( rc != 1 ) 
		{
			err ("failed\n");
			SSL_CTX_free( local_ctx );
			return ERROR;
		}
		rc = SSL_CTX_check_private_key( local_ctx );
		if( rc != 1 ) 
		{
			err ("error\n");
			SSL_CTX_free( local_ctx );
			return ERROR;
		}
		ctx_server = local_ctx;
	}
	*s = ctx_server;
	return OK;
}

status ssl_create_connection( connection_t * c, uint32 flag )
{
	ssl_connection_t * sslcon = NULL;
	int rc = 0;

	if ( (flag != L_SSL_CLIENT) && (flag != L_SSL_SERVER) ) 
	{
		err(" flag not support\n" );
		return ERROR;
	}

	do 
	{
		sslcon = l_safe_malloc( sizeof(ssl_connection_t) );
		if( !sslcon )
		{
			err("alloc sslcon failed, [%d]\n", errno );
			return ERROR;
		}
		memset( sslcon, 0, sizeof(ssl_connection_t) );
		
		rc = ( (flag == L_SSL_CLIENT) ? ssl_client_ctx( &sslcon->session_ctx ) : ssl_server_ctx( &sslcon->session_ctx ) );
		if( OK != OK )
		{
			err("ssl con ctx init failed\n");
			break;
		}

		sslcon->con = SSL_new( sslcon->session_ctx );
		if( NULL == sslcon->con )
		{
			err("ssl5 ssl new failed\n");
			break;
		}

		if( 0 == SSL_set_fd( sslcon->con, c->fd ) )
		{
			err("ssl set fd failed\n");
			break;
		}
		( flag == L_SSL_CLIENT ) ? SSL_set_connect_state( sslcon->con ) : SSL_set_accept_state( sslcon->con );

		if( 0 == SSL_set_ex_data( sslcon->con, 0, c ) )
		{
			err("ssl set ext data failed\n");
			break;
		}
		
		c->ssl = sslcon;
		sslcon->data = c;
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

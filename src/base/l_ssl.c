#include "lk.h"

static SSL_CTX * ctx_client = NULL;
static SSL_CTX * ctx_server = NULL;
static status ssl_write_handler( event_t * event );
static status ssl_read_handler( event_t * event );
static int32  ssl_con_index;

// ssl_shutdown_handler -----------------
status ssl_shutdown_handler( event_t * ev )
{
	connection_t * c;
	ssl_handshake_pt  pt;

	c = ev->data;
	pt = c->ssl->handler;

	if( ssl_shutdown( c->ssl ) == AGAIN ) {
		return AGAIN;
	}
	return pt( c->read );
}
// ssl_shutdown -------------------------
status ssl_shutdown( ssl_connection_t * ssl )
{
	int32 n, ssler = 0;
	connection_t * c;

	c = ssl->data;
	if( SSL_in_init( c->ssl->con ) ) {
		SSL_free( c->ssl->con );
		l_safe_free( c->ssl );
		c->ssl = NULL;
		c->ssl_flag = 0;
		return OK;
	}
	n = SSL_shutdown( c->ssl->con );
	if( n != 1 && ERR_peek_error() ) {
		ssler = SSL_get_error(c->ssl->con, n );
	}
	if( n == 1 || ssler == 0 || ssler == SSL_ERROR_ZERO_RETURN ) {
		SSL_free( c->ssl->con );
		l_safe_free( c->ssl );
		c->ssl = NULL;
		c->ssl_flag = 0;
		return OK;
	}
	if( ssler == SSL_ERROR_WANT_READ ) {
		c->write->handler = NULL;
		c->read->handler = ssl_shutdown_handler;
		event_opt( c->read, EVENT_READ );
		return AGAIN;
	} else if( ssler == SSL_ERROR_WANT_WRITE ) {
		c->read->handler = NULL;
		c->write->handler = ssl_shutdown_handler;
		event_opt( c->write, EVENT_WRITE );
		return AGAIN;
	}
	SSL_free( c->ssl->con );
	l_safe_free( c->ssl );
	c->ssl = NULL;
	c->ssl_flag = 0;
	return ERROR;
}
// ssl_handshake_handler ----------------
static status ssl_handshake_handler( event_t * ev )
{
	connection_t * c;

	c = ev->data;

	if( ssl_handshake( c->ssl ) == AGAIN ) {
		return AGAIN;
	}
	return c->ssl->handler( ev );
}
// ssl_handshake ------------------------
status ssl_handshake( ssl_connection_t * ssl )
{
	int n, ssler;
	connection_t * c;

	c = ssl->data;

	n = SSL_do_handshake( ssl->con );
	if( n == 1 ) {
		ssl->handshaked = 1;
		debug_log ( "%s --- success", __func__ );
		return OK;
	}
	ssler = SSL_get_error( ssl->con, n );
	if( ssler == SSL_ERROR_WANT_READ || ssler == SSL_ERROR_WANT_WRITE ) {
		if( ssler == SSL_ERROR_WANT_READ ) {
			c->write->handler = NULL;
			c->read->handler = ssl_handshake_handler;
			event_opt( c->read, EVENT_READ );
			debug_log ( "%s --- want read", __func__ );
		} else {
			c->read->handler = NULL;
			c->write->handler = ssl_handshake_handler;
			event_opt( c->write, EVENT_WRITE );
			debug_log ( "%s --- want write", __func__ );
		}
		return AGAIN;
	}
	if( ssler == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 ) {
		err_log ( "%s --- peer closed", __func__ );
		return ERROR;
	}
	return ERROR;
}
// ssl_get_client_ctx -------------------
status ssl_client_ctx( SSL_CTX ** s )
{
	if( !ctx_client ) {
		ctx_client = SSL_CTX_new( SSLv23_client_method() );
	}
	*s = ctx_client;
	return OK;
}
// ssl_get_server_ctx -------------------
status ssl_server_ctx( SSL_CTX ** s )
{
	int32 rc;
	char crt[L_SSL_CERT_PATH_LEN] = {0};
	char key[L_SSL_KEY_PATH_LEN] = {0};

	if( !ctx_server ) {
		ctx_server = SSL_CTX_new( SSLv23_server_method() );
		l_memcpy( crt, conf.sslcrt.data, conf.sslcrt.len );
		l_memcpy( key, conf.sslkey.data, conf.sslkey.len );
		rc = SSL_CTX_use_certificate_chain_file (ctx_server, crt);
		if( rc != 1 ) {
			err_log ( "%s --- [SSL_CTX_use_certificate_chain_file] failed", __func__ );
			return ERROR;
		}
		rc = SSL_CTX_use_PrivateKey_file( ctx_server, key, SSL_FILETYPE_PEM );
		if( rc != 1 ) {
			err_log ( "%s --- [SSL_CTX_use_PrivateKey_file] failed", __func__ );
			return ERROR;
		}
		rc = SSL_CTX_check_private_key( ctx_server );
		if( rc != 1 ) {
			err_log ( "%s --- [SSL_CTX_check_private_key] error", __func__ );
			return ERROR;
		}
	}
	*s = ctx_server;
	return OK;
}
//  ssl_read ----------------------------
ssize_t ssl_read( connection_t * c, char * start, uint32 len )
{
	int rc, sslerr;

	rc = SSL_read( c->ssl->con, start, (int)len );
	if( rc > 0 ) {
		if( c->ssl->old_write_handler ) {
			c->write->handler = c->ssl->old_write_handler;
			c->ssl->old_write_handler = NULL;
		}
		return rc;
	}
	sslerr = SSL_get_error( c->ssl->con, rc );
	if( sslerr == SSL_ERROR_WANT_READ ) {
		return AGAIN;
	}
	if( sslerr == SSL_ERROR_WANT_WRITE ) {
		if( NULL == c->ssl->old_write_handler ) {
			c->ssl->old_write_handler = c->write->handler;
			c->write->handler = ssl_write_handler;
		}
		return AGAIN;
	}
	if( sslerr == SSL_ERROR_SYSCALL ) {
		err_log("%s --- ssl error, syserror [%d]", __func__, errno );
	} else {
		err_log("%s --- ssl error, ssl fault", __func__ );
	}
	if ( sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0 ) {
		return ERROR;
	}
	return ERROR;
}
// ssl_write_handler ---------------------
static status ssl_write_handler( event_t * event )
{
	connection_t * c;

	c = event->data;

	return c->read->handler( c->read );
}
// ssl_write ------------------------------
ssize_t ssl_write( connection_t * c, char * start, uint32 len )
{
	int rc, sslerr;

	rc = SSL_write( c->ssl->con, start, (int)len );
	if( rc > 0 ) {
		if( c->ssl->old_read_handler ) {
			c->read->handler = c->ssl->old_read_handler;
			c->ssl->old_read_handler = NULL;
		}
		return rc;
	}
	sslerr = SSL_get_error( c->ssl->con, rc );
	if( sslerr == SSL_ERROR_WANT_WRITE ) {
		return AGAIN;
	}
	if( sslerr == SSL_ERROR_WANT_READ ) {
		if( NULL == c->ssl->old_read_handler ) {
			c->ssl->old_read_handler = c->read->handler;
			c->read->handler = ssl_read_handler;
		}
		return AGAIN;
	}
	if( sslerr == SSL_ERROR_SYSCALL ) {
		err_log("%s --- ssl error, syserror [%d]", __func__, errno );
	} else {
		err_log("%s --- ssl error, ssl fault", __func__ );
	}
	return ERROR;
}
// ssl_read_handler ---------------------
static status ssl_read_handler( event_t * event )
{
	connection_t * c;

	c = event->data;
	return c->write->handler( c->write );
}
// ssl_write_chain ----------------------
status ssl_write_chain( connection_t * c, meta_t * meta )
{
	ssize_t sent;
	meta_t * cl;

	cl = meta;
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
			err_log ( "%s --- ssl write", __func__ );
			return ERROR;
		} else if ( AGAIN == sent ) {
			return AGAIN;
		} else {
			cl->pos += sent;
		}
	}
}
// ssl_create_connection --------------------
status ssl_create_connection( connection_t * c, uint32 flag )
{
	ssl_connection_t * sc = NULL;
	SSL_CTX * ctx;

	if ( (flag != L_SSL_CLIENT) && (flag != L_SSL_SERVER) ) {
		err_log("%s --- flag not support", __func__ );
		return ERROR;
	}
	sc = (ssl_connection_t*)l_safe_malloc( sizeof(ssl_connection_t) );
	if( !sc ) {
		err_log ( "%s --- malloc ssl connection", __func__ );
		return ERROR;
	}
	memset( sc, 0, sizeof(ssl_connection_t) );
	if( flag == L_SSL_CLIENT ) {
		ssl_client_ctx( &ctx );
	} else {
		ssl_server_ctx( &ctx );
	}
	sc->session_ctx = ctx;
	sc->con = SSL_new( sc->session_ctx );
	if( !sc->con ) {
		err_log ( "%s --- SSL_new failed", __func__ );
		l_safe_free( sc );
		return ERROR;
	}
	sc->data = (void*)c;

	if( SSL_set_fd( sc->con, c->fd ) == 0 ) {
		err_log ( "%s --- SSL_set_fd failed", __func__ );
		SSL_free( sc->con );
		l_safe_free( sc );
		return ERROR;
	}
	if( flag == L_SSL_CLIENT ) {
		SSL_set_connect_state( sc->con );
	} else {
		SSL_set_accept_state( sc->con );
	}
	if( SSL_set_ex_data( sc->con, ssl_con_index, c ) == 0 ) {
		err_log ( "%s --- SSL_set_ex_data failed", __func__ );
		SSL_free( sc->con );
		l_safe_free( sc );
		return ERROR;
	}
	c->ssl = sc;
	return OK;
}
// ssl_init -----------------------------
status ssl_init( void )
{
	SSL_library_init( );
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	return OK;
}
// ssl_end ------------------------------
status ssl_end( void )
{
	ERR_free_strings();
	EVP_cleanup();
	return OK;
}

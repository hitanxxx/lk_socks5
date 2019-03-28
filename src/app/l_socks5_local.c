#include "lk.h"

// socks5_remote_handshake ------
static status socks5_remote_handshake( event_t * ev )
{
	connection_t* up;
	socks5_cycle_t * cycle;
	
	up = ev->data;
	cycle = up->data;
	if( !up->ssl->handshaked ) {
		err_log( "%s --- handshake error", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	timer_del( &cycle->up->write->timer );

	cycle->up->recv = ssl_read;
	cycle->up->send = ssl_write;
	cycle->up->recv_chain = NULL;
	cycle->up->send_chain = ssl_write_chain;

	cycle->up->read->handler = NULL;
	cycle->up->write->handler = socks5_pipe;
	return cycle->up->write->handler( cycle->up->write );
}
// socks5_local_connect_test -----
static status socks5_local_connect_test( connection_t * c )
{
	int	err = 0;
    socklen_t  len = sizeof(int);

	if (getsockopt( c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) {
		err = errno;
	}
	if (err) {
		err_log("%s --- remote connect test, [%d]", __func__, errno );
		return ERROR;
	}
	return OK;
}
// socks5_local_connect_check ----
static status socks5_local_connect_check( event_t * ev )
{
	socks5_cycle_t * cycle;
	connection_t* up;
	status rc;

	up = ev->data;
	cycle = up->data;
	if( OK != socks5_local_connect_test( up ) ) {
		err_log( "%s --- socks5 local connect failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	debug_log ( "%s --- connect success", __func__ );
	net_nodelay( up );
	timer_del( &up->write->timer );
	
	cycle->up->ssl_flag = 1;
	if( OK != ssl_create_connection( cycle->up, L_SSL_CLIENT ) ) {
		err_log( "%s --- client upstream ssl create", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	rc = ssl_handshake( cycle->up->ssl );
	if( rc == ERROR ) {
		err_log( "%s --- client upstream ssl handshake", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if ( rc == AGAIN ) {
		up->ssl->handler = socks5_remote_handshake;
		up->write->timer.data = (void*)cycle;
		up->write->timer.handler = socks5_cycle_time_out;
		timer_add( &up->write->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}
	return socks5_remote_handshake( cycle->up->write );
}
// socks5_local_connect_start ------
static status socks5_local_connect_start( event_t * ev )
{
	status rc;
	socks5_cycle_t * cycle;
	connection_t* up;

	up = ev->data;
	cycle = up->data;
	rc = event_connect( up->write );
	if( rc == ERROR ) {
		err_log( "%s --- connect error", __func__ );
		socks5_cycle_free(cycle);
		return ERROR;
	}
	up->write->handler = socks5_local_connect_check;
	event_opt( up->write, EVENT_WRITE );

	if( rc == AGAIN ) {
		debug_log("%s --- connect again", __func__ );
		up->write->timer.data = (void*)cycle;
		up->write->timer.handler = socks5_cycle_time_out;
		timer_add( &up->write->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}
	return up->write->handler( up->write );
}
// socks5_local_remote_init --------
static status socks5_local_remote_init( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;
	
	down = ev->data;
	cycle = down->data;
	if( OK != net_alloc( &cycle->up ) ) {
		debug_log ( "%s --- upstream alloc", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->send = sends;
	cycle->up->recv = recvs;
	cycle->up->send_chain = send_chains;
	cycle->up->recv_chain = NULL;
	cycle->up->data = (void*)cycle;

	if( !cycle->up->meta ) {
		if( OK != meta_alloc( &cycle->up->meta, 4096 ) ) {
			err_log( "%s --- upstream meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	
	struct addrinfo * res = NULL;
	string_t port = string("3333");
	debug_log("%s --- ip [%.*s] port [%.*s]", __func__, 
		conf.socks5_serverip.len, conf.socks5_serverip.data,
		port.len, port.data 
		);
	res = net_get_addr( &conf.socks5_serverip, &port );
	if( !res ) {
		err_log("%s --- net get addr failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	memset( &cycle->up->addr, 0, sizeof(struct sockaddr_in) );
	memcpy( &cycle->up->addr, res->ai_addr, sizeof(struct sockaddr_in) );
	freeaddrinfo( res );
	
	cycle->down->read->handler = NULL;
	cycle->down->write->handler = NULL;

	cycle->up->read->handler = NULL;
	cycle->up->write->handler = socks5_local_connect_start;

	return cycle->up->write->handler( cycle->up->write );
}
// socks5_local_init_connection --------
static status socks5_local_init_connection( event_t * ev )
{
	connection_t * c;
	socks5_cycle_t * cycle;
	
	c = ev->data;
	
	cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
	if( !cycle ) {
		err_log("%s --- malloc socks5 cycle failed", __func__ );
		net_free( c );
		return ERROR;
	}
	memset( cycle, 0, sizeof(socks5_cycle_t) );
	cycle->down = c;
	c->data = (void*)cycle;
	if( !c->meta ) {
		if( OK != meta_alloc( &c->meta, 4096 ) ) {
			err_log( "%s --- c header meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	c->read->handler = socks5_local_remote_init;
	return c->read->handler( c->read );
}
// socks5_local_init --------
status socks5_local_init( void )
{
	if( conf.socks5_mode == SOCKS5_CLIENT ) {
		listen_add( 1080, socks5_local_init_connection, TCP );
	}
	return OK;
}
// socks5_local_end --------
status socks5_local_end( void )
{
	return OK;
}
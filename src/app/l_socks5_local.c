#include "l_base.h"
#include "l_dns.h"
#include "l_socks5_local.h"
#include "l_socks5_server.h"

static status socks5_local_authorization_resp_check( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;

	auth = (socks5_auth_t*)up->meta->pos;
	if( auth->magic != SOCKS5_AUTH_MAGIC_NUM )
	{
		err("check magic [%d] failed, magic [%d]\n", auth->magic, SOCKS5_AUTH_MAGIC_NUM );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	if( auth->message_type != SOCKS5_AUTH_RESP )
	{
		err("check message_type [%d] failed, not SOCKS5_AUTH_RESP [%d]\n", auth->message_type );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	if( auth->message_status != SOCKS5_AUTH_SUCCESS )
	{
		err("check message_status [%d] failed, not success\n", auth->message_status );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	ev->read_pt  = NULL;
	ev->write_pt = socks5_pipe;
	return ev->write_pt( ev );
}

static status socks5_local_authorization_resp_recv( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	
	ssize_t rc;

	while( meta_len( up->meta->pos, up->meta->last ) < sizeof(socks5_auth_t) )
	{
		// recv auth data structrue
		rc = up->recv( up, up->meta->last, meta_len( up->meta->last, up->meta->end ) );
		if( rc < 0 )
		{
			if( rc == AGAIN )
			{
				timer_set_data( &ev->timer, cycle );
				timer_set_pt(&ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err("socks5 local auth resp recv failed\n");
			socks5_cycle_free( cycle );
			return ERROR;
		}
		up->meta->last += rc;
	}
	timer_del( &ev->timer );
	
	up->event.write_pt = socks5_local_authorization_resp_check;
	return ev->write_pt( ev );
}

static status socks5_local_authorization_req_send( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc;

	rc = up->send_chain( up, up->meta );
	if( rc == ERROR )
	{
		err("auth send failed\n");
		socks5_cycle_free( cycle );
		return ERROR;
	}
	else if ( rc == DONE )
	{
		timer_del( &ev->timer );
		
		up->meta->last = up->meta->pos = up->meta->start;
		
		ev->write_pt = NULL;
		ev->read_pt  = socks5_local_authorization_resp_recv;
		event_opt( &cycle->up->event, cycle->up->fd, EV_R );
		
		return ev->read_pt( ev );
	}
	
	timer_set_data( &ev->timer, cycle );
	timer_set_pt(&ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	return AGAIN;
}

static status socks5_local_authorization_req_build( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;

	if( !up->meta ) 
	{
		if( OK != meta_alloc( &up->meta, SOCKS5_META_LENGTH ) ) 
		{
			err("socks5 local auth begin up conn meta alloc\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	
	auth = (socks5_auth_t*)up->meta->last;

	auth->magic = SOCKS5_AUTH_MAGIC_NUM;
	strncpy( auth->name, 	conf.socks5.client.user, sizeof(auth->name) );
	strncpy( auth->passwd, 	conf.socks5.client.passwd, 	sizeof(auth->passwd) );
	auth->message_type	= SOCKS5_AUTH_REQ;
	auth->message_status = 0;
	up->meta->last += sizeof(socks5_auth_t);
	
	ev->write_pt = socks5_local_authorization_req_send;
	return ev->write_pt( ev );
}

static status socks5_local_server_connect_handshake( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	
	if( !up->ssl->handshaked ) {
		err(" handshake error\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	timer_del( &ev->timer );

	up->recv = ssl_read;
	up->send = ssl_write;
	up->recv_chain = NULL;
	up->send_chain = ssl_write_chain;

	ev->write_pt = socks5_local_authorization_req_build;
	return ev->write_pt( ev );
}

static status socks5_local_server_connect_check( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc;

	if( OK != l_socket_check_status( up->fd ) ) 
	{
		err(" socks5 local connect failed\n" );
		goto failed;
	}
	timer_del( &ev->timer );

	l_socket_nodelay( up->fd );

	if( up->ssl_flag == 1 ) 
	{
		if( OK != ssl_create_connection( up, L_SSL_CLIENT ) ) 
		{
			err(" client upstream ssl create\n" );
			goto failed;
		}
		
		rc = ssl_handshake( up->ssl );
		if( rc < 0 ) 
		{
			if( rc == AGAIN ) 
			{
				up->ssl->handler = socks5_local_server_connect_handshake;
				timer_set_data( &ev->timer, (void*)cycle );
				timer_set_pt( &ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err(" client upstream ssl handshake\n" );
			goto failed;
		}
		return socks5_local_server_connect_handshake( ev );
	}
	ev->write_pt = socks5_local_authorization_req_build;
	
	return ev->write_pt( ev );
failed:
	socks5_cycle_free( cycle );
	return ERROR;
}

static inline void socks5_local_server_addr_get( struct sockaddr_in * addr )
{
	memset( addr, 0, sizeof(struct sockaddr_in) );
	addr->sin_family 		= AF_INET;
	addr->sin_port 			= htons( (uint16_t)conf.socks5.client.server_port );
	addr->sin_addr.s_addr	= inet_addr( conf.socks5.client.server_ip );
}

static status socks5_local_cycle_init( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = NULL;
	status rc;

	down->event.read_pt = NULL;
	down->event.write_pt = NULL;
	
	// init struct data
	cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
	if( !cycle ) 
	{
		err(" malloc socks5 cycle failed\n" );
		net_free( down );
		return ERROR;
	}
	memset( cycle, 0, sizeof(socks5_cycle_t) );
	
	down->data = (void*)cycle;
	cycle->down = down;

	if( OK != net_alloc( &cycle->up ) ) 
	{
		err(" up con alloc\n" );
		goto failed;
	}
	cycle->up->data = (void*)cycle;
	// lks5 local use ssl encrypt connect to server
	cycle->up->ssl_flag = 1;
	socks5_local_server_addr_get( &cycle->up->addr );

	// goto connect
	rc = l_net_connect( cycle->up, &cycle->up->addr, TYPE_TCP );
	if( ERROR == rc ) 
	{
		err(" up connect failed\n" );
		goto failed;
	}
	cycle->up->event.write_pt	= NULL;
	cycle->up->event.write_pt 	= socks5_local_server_connect_check;
	event_opt( &cycle->up->event, cycle->up->fd, EV_W );
	
	if( AGAIN == rc ) 
	{
		timer_set_data( &cycle->up->event.timer, (void*)cycle );
		timer_set_pt( &cycle->up->event.timer, socks5_timeout_cycle );
		timer_add( &cycle->up->event.timer, SOCKS5_TIME_OUT );
		return rc;
	}
	return cycle->up->event.write_pt( &cycle->up->event );
failed:
	socks5_cycle_free( cycle );
	return ERROR;
}

status socks5_local_init( void )
{
	if( conf.socks5_mode == SOCKS5_CLIENT )
	{
		listen_add( conf.socks5.client.local_port, socks5_local_cycle_init, L_NOSSL );
	}
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}


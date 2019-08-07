#include "lk.h"

static status socks5_local_private_auth_resp( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;
	ssize_t rc;
	
	while(1)
	{
		// recv auth data structrue
		if( meta_len( up->meta->pos, up->meta->last ) < sizeof(socks5_auth_t) )
		{
			rc = up->recv( up, up->meta->pos, meta_len( up->meta->last, up->meta->end ) );
			if( rc < 0 )
			{
				if( rc == AGAIN )
				{
					return AGAIN;
				}
				err("socks5 local auth resp recv failed\n");
				socks5_cycle_free( cycle );
				return ERROR;
			}
			up->meta->last += rc;
		}

		// check auth res
		timer_del( &ev->timer );

		auth = (socks5_auth_t*)up->meta->pos;
		if( auth->magic != 947085 )
		{
			err("check magic failed, [%d] != magic [947085]\n", auth->magic );
			socks5_cycle_free( cycle );
			return ERROR;
		}
		if( auth->auth_type != SOCKS5_AUTH_RESP )
		{
			err("check auth_type failed, [%x] not SOCKS5_AUTH_RESP\n", auth->auth_type );
			socks5_cycle_free( cycle );
			return ERROR;
		}
		
		if( auth->auth_resp_code != SOCKS5_AUTH_SUCCESS )
		{
			err("check auth_resp_code not success, [%x]\n", auth->auth_resp_code );
			socks5_cycle_free( cycle );
			return ERROR;
		}
		ev->read_pt  = NULL;
		ev->write_pt = socks5_pipe;
		
		return ev->write_pt( ev );
	}
}

static status socks5_local_private_auth_send( event_t * ev )
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
		event_opt( &cycle->up->event, cycle->up->fd, EV_R );
		ev->read_pt  = socks5_local_private_auth_resp;

		timer_set_data( &ev->timer, cycle );
		timer_set_pt(&ev->timer, socks5_timeout_cycle );
		// socks5_timeout_cycle seconds for recv complated scoks5 auth data
		
		return ev->read_pt( ev );
	}
	
	timer_set_data( &ev->timer, cycle );
	timer_set_pt(&ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	return AGAIN;
}

static status socks5_local_private_auth_begin( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;

	if( !up->meta ) 
	{
		if( OK != meta_alloc( &up->meta, 4096 ) ) 
		{
			err("socks5 local auth begin up conn meta alloc\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	auth = (socks5_auth_t*)up->meta->pos;
	memset( auth, 0, sizeof(socks5_auth_t) );

	auth->magic = 947085;
	strncpy( auth->name, 	conf.socks5_client_user.data, 	conf.socks5_client_user.len );
	strncpy( auth->passwd, 	conf.socks5_client_passwd.data, 	conf.socks5_client_passwd.len );
	auth->auth_type	= SOCKS5_AUTH_REQ;
	auth->auth_resp_code = 0;

	up->meta->last += sizeof(socks5_auth_t);
	
	ev->write_pt = socks5_local_private_auth_send;

	return ev->write_pt( ev );
}

static status socks5_local_connect_handshake( event_t * ev )
{
	connection_t* up;
	socks5_cycle_t * cycle;
	
	up = ev->data;
	cycle = up->data;
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

	ev->write_pt = socks5_local_private_auth_begin;
	
	return ev->write_pt( ev );
}

static status socks5_local_connect_check( event_t * ev )
{
	connection_t* up;
	status rc;
	socks5_cycle_t * cycle;

	up = ev->data;
	cycle = up->data;
	if( OK != l_socket_check_status( up->fd ) ) 
	{
		err(" socks5 local connect failed\n" );
		goto failed;
	}
	l_socket_nodelay( up->fd );
	timer_del( &ev->timer );

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
				up->ssl->handler = socks5_local_connect_handshake;
				timer_set_data( &ev->timer, (void*)cycle );
				timer_set_pt( &ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err(" client upstream ssl handshake\n" );
			goto failed;
		}
		return socks5_local_connect_handshake( ev );
	}
	ev->write_pt = socks5_local_private_auth_begin;
	
	return ev->write_pt( ev );
failed:
	socks5_cycle_free( cycle );
	return ERROR;
}

static status socks5_local_init_connection( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;
	status rc;
	char port_str[32] = {0};
	string_t port_string;

	snprintf( port_str, sizeof(port_str), "%d", conf.socks5_server_port );
	port_string.data = port_str;
	port_string.len = l_strlen(port_str);
	
	down = ev->data;
	cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
	if( !cycle ) 
	{
		err(" malloc socks5 cycle failed\n" );
		net_free( down );
		return ERROR;
	}
	memset( cycle, 0, sizeof(socks5_cycle_t) );

	cycle->down = down;
	down->data = (void*)cycle;

	down->event.read_pt = NULL;
	down->event.write_pt = NULL;
	
	if( OK != net_alloc( &cycle->up ) ) 
	{
		err(" up con alloc\n" );
		goto failed;
	}
	
	cycle->up->ssl_flag = 1;
	cycle->up->data = (void*)cycle;
	
	rc = l_net_connect( cycle->up, &conf.socks5_serverip, &port_string );
	if( ERROR == rc ) 
	{
		err(" up connect failed\n" );
		goto failed;
	}
	
	event_opt( &cycle->up->event, cycle->up->fd, EV_W );
	cycle->up->event.write_pt = socks5_local_connect_check;
	
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
	if( conf.socks5_mode == SOCKS5_CLIENT ) {
		listen_add( conf.socks5_local_port, socks5_local_init_connection, L_NOSSL );
	}
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}


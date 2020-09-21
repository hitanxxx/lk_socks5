#include "l_base.h"
#include "l_dns.h"
#include "l_socks5_local.h"
#include "l_socks5_server.h"

static status socks5_local_authorization_resp_check( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;

	do 
	{
		auth = (socks5_auth_t*) up->meta->pos;

		if( SOCKS5_AUTH_MAGIC_NUM != auth->magic )
		{
			err("s5 local auth check magic failed, [%d]\n", auth->magic );
			break;
		}

		if( S5_AUTH_TYPE_RESP != auth->message_type )
		{
			err("s5 local auth check type failed, [%d]\n", auth->message_type );
			break;
		}

		if( S5_AUTH_STAT_SUCCESS != auth->message_status )
		{
			err("s5 local auth check stat failed, [%d]\n", auth->message_status );
			break;
		}

		ev->read_pt		= NULL;
		ev->write_pt	= socks5_pipe;
		return ev->write_pt( ev );
	} while(0);

	socks5_cycle_free( cycle );
	return ERROR;
}

static status socks5_local_authorization_resp_recv( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	
	ssize_t rc;

	while( meta_len( up->meta->pos, up->meta->last ) < sizeof(socks5_auth_t) )
	{
		rc = up->recv( up, up->meta->last, meta_len( up->meta->last, up->meta->end ) );
		if( rc < 0 )
		{
			if( rc == ERROR )
			{
				err("s5 local authorization recv failed\n");
				socks5_cycle_free( cycle );
				return ERROR;
			}
			timer_set_data( &ev->timer, cycle );
			timer_set_pt(&ev->timer, socks5_timeout_cycle );
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
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
	if( rc < 0 )
	{
		if( rc == ERROR )
		{
			err("s5 local send authorization data failed\n");
			socks5_cycle_free( cycle );
			return ERROR;
		}
		timer_set_data( &ev->timer, cycle );
		timer_set_pt( &ev->timer, socks5_timeout_cycle );
		timer_add( &ev->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}

	timer_del( &ev->timer );
	debug("s5 local send authorization success\n");
	up->meta->last = up->meta->pos = up->meta->start;

	ev->write_pt	= NULL;
	ev->read_pt		= socks5_local_authorization_resp_recv;
	event_opt( ev, up->fd, EV_R );

	return ev->read_pt( ev );
}

static status socks5_local_authorization_req_build( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	socks5_auth_t * auth = NULL;

	if( up->meta == NULL ) 
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
	auth->message_type	= S5_AUTH_TYPE_REQ;
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

	up->recv 		= ssl_read;
	up->send 		= ssl_write;
	up->recv_chain 	= NULL;
	up->send_chain 	= ssl_write_chain;

	ev->write_pt = socks5_local_authorization_req_build;
	return ev->write_pt( ev );
}

static status socks5_local_server_connect_check( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc;

	do
	{
		if( OK != l_socket_check_status( up->fd ) )
		{
			err("s5 local connect failed\n");
			break;
		}
		timer_del( &ev->timer );
		l_socket_nodelay( up->fd );

		if( up->ssl_flag )
		{
			if( OK != ssl_create_connection( up, L_SSL_CLIENT ) )
			{
				err("s5 local create ssl connection for up failed\n");
				break;
			}
			rc = ssl_handshake( up->ssl );
			if( rc < 0 )
			{
				if( rc != AGAIN )
				{
					err("s5 local ssl handshake failed\n");
					break;
				}
				up->ssl->cb 	= socks5_local_server_connect_handshake;
				timer_set_data( &ev->timer, cycle );
				timer_set_pt( &ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			return socks5_local_server_connect_handshake( ev );
		}

		ev->write_pt	= socks5_local_authorization_req_build;
		return ev->write_pt( ev );
	} while(0);

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

	down->event.read_pt 	= NULL;
	down->event.write_pt 	= NULL;	
	do
	{
		cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
		if( !cycle )
		{
			err("malloc socks5 cycle failed, [%d]\n", errno );
			net_free( down );
			return ERROR;
		}
		memset( cycle, 0, sizeof(socks5_cycle_t) );

		down->data 	= cycle;
		cycle->down = down;

		if( OK != net_alloc( &cycle->up ) )
		{
			err("cycle up alloc failed\n");
			break;
		}
		cycle->up->data		= cycle;
		cycle->up->ssl_flag	= 1;

		socks5_local_server_addr_get( &cycle->up->addr );

		rc = l_net_connect( cycle->up, &cycle->up->addr, TYPE_TCP );
		if( rc == ERROR )
		{
			err("cycle up connect failed\n");
			break;
		}
		cycle->up->event.read_pt	= NULL;
		cycle->up->event.write_pt	= socks5_local_server_connect_check;
		event_opt( &cycle->up->event, cycle->up->fd, EV_W );
		if( rc == AGAIN )
		{
			timer_set_data( &cycle->up->event.timer, cycle );
			timer_set_pt( &cycle->up->event.timer, socks5_timeout_cycle );
			timer_add( &cycle->up->event.timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		return cycle->up->event.write_pt( &cycle->up->event );
	} while(0);

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


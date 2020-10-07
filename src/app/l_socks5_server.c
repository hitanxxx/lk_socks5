#include "l_base.h"
#include "l_dns.h"
#include "l_socks5_local.h"
#include "l_socks5_server.h"

typedef struct private_s5
{
    queue_t         usable;
    queue_t         use;
    socks5_cycle_t  pool[0];
} private_s5_t;
static private_s5_t * this = NULL;

static status lks5_down_recv( event_t * ev );
static status lks5_up_send( event_t * ev );
static status lks5_up_recv( event_t * ev );
static status lks5_down_send( event_t * ev );

status socks5_cycle_alloc( socks5_cycle_t ** cycle )
{
    queue_t * q = NULL;
    socks5_cycle_t * new_cycle = NULL;
    
    if( queue_empty(&this->usable) )
    {
        err("s5 alloc usable empty\n");
        return ERROR;
    }
    q = queue_head(&this->usable);
    queue_remove(q);
    
    queue_insert_tail(&this->use, q);
    new_cycle = l_get_struct(q, socks5_cycle_t, queue);
    *cycle = new_cycle;
    return OK;
}

status socks5_cycle_free( socks5_cycle_t * cycle )
{
    queue_t * q = &cycle->queue;
    
    queue_remove( q );
    queue_insert_tail(&this->usable, q);
    return OK;
}

status socks5_cycle_over( socks5_cycle_t * cycle )
{
	if( cycle->up ) 
	{
		net_free( cycle->up );
		cycle->up       = NULL;
	}
	if( cycle->down )
	{
		net_free( cycle->down );
		cycle->down     = NULL;
	}
	if( cycle->dns_cycle )
	{
		l_dns_free( cycle->dns_cycle );
		cycle->dns_cycle = NULL;
	}
    cycle->up_recv_error    = 0;
    cycle->down_recv_error  = 0;
    socks5_cycle_free(cycle);
	return OK;
}

inline void socks5_timeout_cycle( void * data )
{
	socks5_cycle_over( (socks5_cycle_t *)data );
}

inline void socks5_timeout_con( void * data )
{
	net_free( (connection_t *)data );
}

static status lks5_down_recv( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	meta_t * meta = &cycle->down2up_meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	
	if( cycle->down_recv_error != 1 )
	{
		while( meta_len( meta->last, meta->end ) > 0 )
		{
			rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					cycle->down_recv_error = 1;
				}
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
				break;
			}
			meta->last += rc;
		}
	}
	
	if( meta_len( meta->pos, meta->last ) > 0 || cycle->down_recv_error == 1 )
	{
        // disable down recv && enable up send
		event_opt( down->event, down->fd, ( down->event->type & ~EV_R ) );
		event_opt( cycle->up->event, cycle->up->fd, ( cycle->up->event->type | EV_W ) );
		return cycle->up->event->write_pt( cycle->up->event );
	}
	return AGAIN;
}

/*
 * send always send the data that recvd, it will be
 * close the connection when send error.
 */
static status lks5_up_send( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	meta_t * meta = &cycle->down2up_meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( meta_len( meta->pos, meta->last ) > 0 )
	{
		rc = up->send( up, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc < 0 )
		{
			if( rc == ERROR )
			{
				err("up send failed\n");
				socks5_cycle_over( cycle );
				return ERROR;
			}
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		meta->pos += rc;
	}

	if( cycle->down_recv_error == 1 )
	{
		err("down->up remain empty, down recv error, close the socks5 cycle\n" );
		socks5_cycle_over( cycle );
		return ERROR;
	}

	meta->pos = meta->last = meta->start;
    // disable up send && enable down recv
	event_opt( up->event, up->fd, (up->event->type & ~EV_W) );
	event_opt( cycle->down->event, cycle->down->fd, ( cycle->down->event->type | EV_R ) );
	return cycle->down->event->read_pt( cycle->down->event );
}

static status lks5_up_recv( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	meta_t * meta = &cycle->up2down_meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	if( cycle->up_recv_error != 1 )
	{
		while( meta_len( meta->last, meta->end ) > 0 )
		{
			rc = up->recv( up, meta->last, meta_len( meta->last, meta->end ) );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					cycle->up_recv_error = 1;
				}
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
				break;
			}
			meta->last += rc;
		}
	}
	
	if( meta_len( meta->pos, meta->last ) > 0 || cycle->up_recv_error == 1 )
	{
        // disable up recv && enable down send
		event_opt( up->event, up->fd, ( up->event->type & ~EV_R ) );
		event_opt( cycle->down->event, cycle->down->fd, ( cycle->down->event->type | EV_W ) );
		return cycle->down->event->write_pt( cycle->down->event );
	}
	return AGAIN;
}

static status lks5_down_send( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	meta_t * meta = &cycle->up2down_meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( meta_len( meta->pos, meta->last ) > 0 )
	{
		rc = down->send( down, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc < 0 )
		{
			if( rc == ERROR )
			{
				err("down send failed\n");
				socks5_cycle_over( cycle );
				return ERROR;
			}
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		meta->pos += rc;
	}

	if( cycle->up_recv_error == 1 )
	{
		err("up->down remain empty, up recv error, close the socks5 cycle\n");
		socks5_cycle_over( cycle );
		return ERROR;
	}

	meta->pos = meta->last = meta->start;
    // disable down send && enable up recv
	event_opt( down->event, down->fd, (down->event->type & ~EV_W) );
	event_opt( cycle->up->event, cycle->up->fd, (cycle->up->event->type | EV_R) );
	return cycle->up->event->read_pt( cycle->up->event );
}

status socks5_pipe( event_t * ev )
{
	connection_t * c = ev->data;
	socks5_cycle_t * cycle = c->data;
	l_meta_t * meta = NULL;
    
	cycle->down->event->read_pt 	= lks5_down_recv;
	cycle->down->event->write_pt 	= lks5_down_send;

	cycle->up->event->read_pt 		= lks5_up_recv;
	cycle->up->event->write_pt 		= lks5_up_send;
	
	event_opt( cycle->up->event, cycle->up->fd, EV_R );
	event_opt( cycle->down->event, cycle->down->fd, EV_R );

    meta = &cycle->down2up_meta;
    meta->pos = meta->last = meta->start = cycle->down2up_buffer;
    meta->end = meta->start + SOCKS5_META_LENGTH;
    
    meta = &cycle->up2down_meta;
    meta->pos = meta->last = meta->start = cycle->up2down_buffer;
    meta->end = meta->start + SOCKS5_META_LENGTH;

	return cycle->down->event->read_pt( cycle->down->event );
}

static status socks5_server_s5_msg_advance_response_send( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	status rc = 0;

	rc = down->send_chain( down, meta );
	if( rc < 0 )
	{
		if( rc == ERROR )
		{
			err("s5 server msg advance resp send failed\n");
			socks5_cycle_over( cycle );
			return ERROR;
		}
		timer_set_data( &ev->timer, cycle );
		timer_set_pt( &ev->timer, socks5_timeout_cycle );
		timer_add( &ev->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}

	timer_del( &ev->timer );
	ev->read_pt		= socks5_pipe;
	ev->write_pt	= NULL;
	return ev->read_pt( ev );
}

static status socks5_server_s5_msg_advance_response_build( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	connection_t * down = cycle->down;
    meta_t * meta = &cycle->down2up_meta;
	socks5_message_advance_response_t * response = NULL;

	meta->last = meta->pos = meta->start;
	response = ( socks5_message_advance_response_t* )meta->last;
	response->ver 		=	0x05;
	response->rep		=	0x00;
	response->rsv		=	0x00;
	response->atyp		=	0x01;
	response->bnd_addr	=	htons((uint16_t)up->addr.sin_addr.s_addr);
	response->bnd_port	=	htons(up->addr.sin_port);
	meta->last += sizeof(socks5_message_advance_response_t);

	// make up event invalidat
	event_opt( up->event, up->fd, EV_NONE );
	
	event_opt( down->event, down->fd, EV_W );
	down->event->write_pt = socks5_server_s5_msg_advance_response_send;
	return down->event->write_pt( down->event );
}

static status socks5_server_connect_up_check( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;

    if( OK != l_socket_check_status( up->fd ) )
	{
		err("s5 server connect remote failed\n" );
		socks5_cycle_over( cycle );
		return ERROR;
	}
	l_socket_nodelay( up->fd );
	timer_del( &ev->timer );

	ev->read_pt		= NULL;
	ev->write_pt 	= socks5_server_s5_msg_advance_response_build;
	return ev->write_pt( ev );
}

static status socks5_server_connect_up( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc = 0;

	rc = l_net_connect( cycle->up, &cycle->up->addr, TYPE_TCP );
	if( rc == ERROR )
	{
		err("s5 server connect up failed\n");
		socks5_cycle_over( cycle );
		return ERROR;
	}
	ev->read_pt 	= NULL;
	ev->write_pt 	= socks5_server_connect_up_check;
	event_opt( ev, up->fd, EV_W );
	if( rc == AGAIN )
	{
		timer_set_data( &ev->timer, cycle );
		timer_set_pt( &ev->timer, socks5_timeout_cycle );
		timer_add( &ev->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}
	return ev->write_pt( ev );
}

static status socks5_server_connection_try_read( event_t * ev  )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;

	if( OK != l_socket_check_status( down->fd ) )
	{
		err("s5 server check down fd status error\n");
		socks5_cycle_over( cycle );
		return ERROR;
	}
	return OK;
}


static void socks5_server_s5_msg_advance_req_addr_dns_callback( void * data )
{
	socks5_cycle_t * cycle = data;
	dns_cycle_t * dns_cycle = cycle->dns_cycle;
	char ipstr[128] = {0};

	if( dns_cycle )
	{
		if( OK == dns_cycle->dns_status )
		{
			snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
				dns_cycle->answer.rdata->data[0],
				dns_cycle->answer.rdata->data[1],
				dns_cycle->answer.rdata->data[2],
				dns_cycle->answer.rdata->data[3]
			);
			
			cycle->up->addr.sin_family		= AF_INET;
			cycle->up->addr.sin_port 		= *(uint16_t*)cycle->advance.addr_port;
			cycle->up->addr.sin_addr.s_addr = inet_addr( ipstr );
			
			cycle->up->event->read_pt 		= NULL;
			cycle->up->event->write_pt		= socks5_server_connect_up;
			cycle->up->event->write_pt( cycle->up->event );
		}
		else
		{
			err("socks5 server dns resolv failed\n");
			socks5_cycle_over( cycle );
		}
	}
}

static status socks5_server_s5_msg_advance_req_addr( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	char ipstr[128] = {0}, portstr[128] = {0};
	status rc = 0;

	down->event->read_pt 	= socks5_server_connection_try_read;
	down->event->write_pt 	= NULL;
	event_opt( down->event, down->fd, EV_R );

	if( OK != net_alloc( &cycle->up ) ) 
	{
		err("s5 server up alloc failed\n" );
		socks5_cycle_over( cycle );
		return ERROR;
	}
	cycle->up->data = cycle;

	if( cycle->advance.atyp == 0x01 )
	{
		int local_port = 0;
		// ip type address
		snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
			(unsigned char )cycle->advance.addr_str[0],
			(unsigned char )cycle->advance.addr_str[1],
			(unsigned char )cycle->advance.addr_str[2],
			(unsigned char )cycle->advance.addr_str[3] );
		local_port = ntohs(*(uint16_t*)cycle->advance.addr_port);
		snprintf( portstr, sizeof(portstr), "%d", local_port );

		cycle->up->addr.sin_family			= AF_INET;
		cycle->up->addr.sin_port 			= htons( local_port );
		cycle->up->addr.sin_addr.s_addr 	= inet_addr( ipstr );

		cycle->up->event->read_pt 			= NULL;
		cycle->up->event->write_pt 			= socks5_server_connect_up;
		return cycle->up->event->write_pt( cycle->up->event );
	}
	else if ( cycle->advance.atyp == 0x03 )
	{
		// domain type address
		rc = l_dns_create( &cycle->dns_cycle );
		if( rc == ERROR )
		{
			err("s5 server dns cycle create failed\n");
			socks5_cycle_over( cycle );
			return ERROR;
		}
		memcpy( cycle->dns_cycle->query, cycle->advance.addr_str, sizeof(cycle->dns_cycle->query)-1 );
		cycle->dns_cycle->cb 		= socks5_server_s5_msg_advance_req_addr_dns_callback;
		cycle->dns_cycle->cb_data 	= cycle;
		return l_dns_start( cycle->dns_cycle );
	}
	err("s5 server not support socks5 request atyp [%x]\n", cycle->advance.atyp );
	socks5_cycle_over( cycle );
	return ERROR;
}


static status socks5_server_s5_msg_advance_req_recv( event_t * ev )
{
	unsigned char * p = NULL;
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
    
	enum{
		ver = 0,
		cmd,
		rsv,
		atyp,
		dst_addr_ipv4,
		dst_addr_ipv6,
		dst_addr_domain_len,
		dst_addr_domain,
		dst_port,
		dst_port_end
	} state;

	/*
	 s5 msg advance format
		char  char  char  char   ...    char*2
 		VER | CMD | RSV | ATYP | ADDR | PORT 
	 */ 

	while( 1 )
	{
		if( meta_len( meta->pos, meta->last ) <= 0 )
		{
			ssize_t rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					err("s5 server advance req recv failed\n");
					socks5_cycle_over( cycle );
					return ERROR;
				}
				timer_set_data( &ev->timer, (void*)cycle );
				timer_set_pt( &ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			meta->last += rc;
		}

		state = cycle->advance.state;
		for( ; meta->pos < meta->last; meta->pos ++ )
		{
			p = meta->pos;
			if( state == ver )
			{
				/*
					ver always 0x05
				*/
				cycle->advance.ver = *p;
				state = cmd;
				continue;
			}
			if( state == cmd )
			{
				/*
					socks5 support cmd value
					01				connect
					02				bind
					03				udp associate
				*/
				cycle->advance.cmd = *p;
				state = rsv;
				continue;
			}
			if( state == rsv )
			{
				// rsv means resverd
				cycle->advance.rsv = *p;
				state = atyp;
				continue;
			}
			if( state == atyp )
			{
				cycle->advance.atyp = *p;
				/*
					atyp		type		length
					0x01		ipv4		4
					0x03		domain		first octet
					0x04		ipv6		16
				*/
				if( cycle->advance.atyp == 0x01 ) {
					state = dst_addr_ipv4;
					cycle->advance.addr_recv = 0;
					continue;
				}
				else if ( cycle->advance.atyp == 0x04 )
				{
					state = dst_addr_ipv6;
					cycle->advance.addr_recv = 0;
					continue;
				}
				else if ( cycle->advance.atyp == 0x03 )
				{
					state = dst_addr_domain_len;
					cycle->advance.addr_recv 	= 0;
					cycle->advance.addr_len 	= 0;
					continue;
				}
				err("s5 server request atyp [%d] not support\n", cycle->advance.atyp );
				socks5_cycle_over( cycle );
				return ERROR;
			}
			if( state == dst_addr_ipv4 )
			{
				cycle->advance.addr_str[(int)cycle->advance.addr_recv++] = *p;
				if( cycle->advance.addr_recv == 4 )
				{
					state = dst_port;
					continue;
				}
			}
			if( state == dst_addr_ipv6 )
			{
				cycle->advance.addr_str[(int)cycle->advance.addr_recv++] = *p;
				if( cycle->advance.addr_recv == 16 )
				{
					state = dst_port;
					continue;
				}
			}
			if( state == dst_addr_domain_len )
			{
				cycle->advance.addr_len = l_max( *p, 0 );
				state = dst_addr_domain;
				continue;
			}
			if( state == dst_addr_domain )
			{
				cycle->advance.addr_str[(int)cycle->advance.addr_recv++] = *p;
				if( cycle->advance.addr_recv == cycle->advance.addr_len )
				{
					state = dst_port;
					continue;
				}
			}
			if( state == dst_port )
			{
				cycle->advance.addr_port[0] = *p;
				state = dst_port_end;
				continue;
			}
			if( state == dst_port_end )
			{
				cycle->advance.addr_port[1] = *p;

				if( cycle->advance.cmd != 0x01 ) 
				{
					err("s5 server advance req only support CMD `connect` 0x01, [%d]\n", *p );
					socks5_cycle_over( cycle );
					return ERROR;
				}
				if( cycle->advance.atyp == 0x04 )
				{
					err("s5 server advance req not support ipv6 request, atype [%d]\n", cycle->advance.atyp );
					socks5_cycle_over( cycle );
					return ERROR;
				}
				cycle->advance.state    = 0;
				timer_del( &ev->timer );

				ev->write_pt 	= NULL;
				ev->read_pt  	= socks5_server_s5_msg_advance_req_addr;
				return ev->read_pt( ev );
			}
		}
		cycle->advance.state = state;
	}
}

static status socks5_server_s5_msg_invate_resp_send ( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	status rc = 0;

	rc = down->send_chain( down, meta );
	if( rc < 0 )
	{
		if( rc == ERROR )
		{
			err("s5 server invate resp send failed\n");
			socks5_cycle_over( cycle );
			return ERROR;
		}
		timer_set_data( &ev->timer, cycle );
		timer_set_pt(&ev->timer, socks5_timeout_cycle );
		timer_add( &ev->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}

	timer_del( &ev->timer );
	meta->pos       = meta->last = meta->start;

	ev->read_pt		= socks5_server_s5_msg_advance_req_recv;
	ev->write_pt	= NULL;
	event_opt( ev, down->fd, EV_R );
	return ev->read_pt( ev );
}

static status socks5_server_s5_msg_invate_resp_build( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	socsk5_message_invite_response_t * invite_resp = NULL;
	/*
		s5 msg invate resp format
		1 byte 		1 byte
		version | ack authentication method
	*/
	meta->pos = meta->last = meta->start;
	invite_resp             = ( socsk5_message_invite_response_t* ) meta->pos;
	invite_resp->ver	 	= 0x05;
	invite_resp->method 	= 0x00;
	meta->last += sizeof(socsk5_message_invite_response_t);

	ev->read_pt 	= NULL;
	ev->write_pt 	= socks5_server_s5_msg_invate_resp_send;
	event_opt( ev, down->fd, EV_W );
	return ev->write_pt( ev );
}

static status socks5_server_s5_msg_invate_req_recv( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	unsigned char * p = NULL;
	/*
		s5 invite message req format
		1 byte		1 byte					1-255 byte
		version | authentication method num | auth methods
	*/
	enum 
	{
		ver = 0,
		nmethod,
		methods
	} state;

	state = cycle->invite.state;
	while( 1 )
	{
		if( meta_len( meta->pos, meta->last ) <= 0 )
		{
			ssize_t rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					err("s5 server invate recv failed\n");
					socks5_cycle_over( cycle );
					return ERROR;
				}
				timer_set_data( &ev->timer, (void*)cycle );
				timer_set_pt( &ev->timer, socks5_timeout_cycle );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			meta->last += rc;
		}

		for( ; meta->pos < meta->last; meta->pos ++ )
		{
			p = meta->pos;
			if( state == ver )
			{
				cycle->invite.ver = *p;
				state = nmethod;
				continue;
			}
			if( state == nmethod )
			{
				cycle->invite.method_num = *p;
				cycle->invite.method_n = 0;
				state = methods;
				continue;
			}
			if( state == methods )
			{
				cycle->invite.method[(int)cycle->invite.method_n++] = *p;
				if( cycle->invite.method_n == cycle->invite.method_num )
				{
					// recv auth over
					timer_del( &ev->timer );

					ev->read_pt = socks5_server_s5_msg_invate_resp_build;
					return ev->read_pt(ev);
				}
			}
		}
		cycle->invite.state = state;
	}
}

static status socks5_server_cycle_init( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	socks5_auth_t * auth = NULL;
    
    // if s5 server check auth data failed, stop the connection
    auth = (socks5_auth_t*)meta->start;
    if( S5_AUTH_STAT_SUCCESS != auth->message_status )
    {
        err("s5 auth check failed, simple finsih s5 cycle now\n");
        net_free( down );
        return ERROR;
    }
    
	meta->pos       = meta->last = meta->start;
	ev->write_pt 	= NULL;
	ev->read_pt  	= socks5_server_s5_msg_invate_req_recv;
	event_opt( ev, down->fd, EV_R );
	return ev->read_pt( ev );
}

static status socks5_server_authorization_resp_send( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	status rc;

	rc = down->send_chain( down, meta );
	if( rc < 0 )
	{
		if( rc == ERROR )
		{
			err("s5 server auth resp send failed\n");
            socks5_cycle_over(cycle);
			return ERROR;
		}
		timer_set_data( &ev->timer, down );
		timer_set_pt( &ev->timer, socks5_timeout_con );
		timer_add( &ev->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}

	timer_del( &ev->timer );
    
	ev->write_pt	= socks5_server_cycle_init;
	return ev->write_pt( ev );
}

static status socks5_server_authorization_resp_build( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = down->data;
    meta_t * meta = &cycle->down2up_meta;
	socks5_auth_t * auth = NULL;
	status rc = 0;
	string_t req_name = string_null, req_passwd = string_null;
	user_t * user = NULL;

	do 
	{
		auth = (socks5_auth_t *)meta->pos;
		if( SOCKS5_AUTH_MAGIC_NUM != auth->magic )
		{
			err("s5 server check auth magic failed, [%d]\n", auth->magic );
			rc = S5_AUTH_STAT_MAGIC_FAIL;
			break;
		}
		if( S5_AUTH_TYPE_REQ != auth->message_type )
		{
			err("s5 server check auth type failed, [%d]\n", auth->message_type );
			rc = S5_AUTH_STAT_TYPE_FAIL;
			break;
		}
		req_name.len 	= l_strlen((char*)auth->name);
		req_name.data	= auth->name;

		req_passwd.len	= l_strlen((char*)auth->passwd);
		req_passwd.data = auth->passwd;

		if( OK != user_find( &req_name, &user ) )
		{
			err("s5 server not found user, [%.*s]\n", req_name.len, req_name.data );
			rc = S5_AUTH_STAT_NO_USER;
			break;
		}

		if( l_strlen(user->passwd) != req_passwd.len || memcmp( user->passwd, req_passwd.data, req_passwd.len ) != 0 )
		{
			err("s5 server check auth passwd failed\n");
			rc = S5_AUTH_STAT_PASSWD_FAIL;
			break;
		}
		rc = S5_AUTH_STAT_SUCCESS;
	} while(0);

	auth->message_type		= S5_AUTH_TYPE_RESP;
	auth->message_status	= rc;
    meta->last              = meta->pos + sizeof(socks5_auth_t);
    
	ev->read_pt		        = NULL;
	ev->write_pt	        = socks5_server_authorization_resp_send;
	event_opt( down->event, down->fd, EV_W );
	return ev->write_pt( ev );
}

static status socks5_server_authorization_req_recv( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = down->data;
	meta_t * meta = &cycle->down2up_meta;
	ssize_t rc = 0;

	while( meta_len( meta->pos, meta->last ) < sizeof(socks5_auth_t) )
	{
		rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
		if( rc < 0 )
		{
			if( ERROR == rc )
			{
				err("s5 server authorizaton recv failed\n");
                socks5_cycle_over(cycle);
				return ERROR;
			}
			timer_set_data( &ev->timer, down );
			timer_set_pt(&ev->timer, socks5_timeout_con );
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		down->meta->last += rc;
	}
	timer_del( &ev->timer );
	
	ev->read_pt = socks5_server_authorization_resp_build;
	return ev->read_pt( ev );
}

static status socks5_server_authorization_start( event_t * ev )
{
	connection_t * down = ev->data;
    socks5_cycle_t * cycle = NULL;
    meta_t * meta = NULL;
    
    if( OK != socks5_cycle_alloc(&cycle) )
    {
        err("s5 server alloc cycle failed\n");
        net_free( down );
        return ERROR;
    }
    cycle->down = down;
    down->data  = cycle;
    
    meta = &cycle->down2up_meta;
    meta->pos = meta->last = meta->start = cycle->down2up_buffer;
    meta->end = meta->start + SOCKS5_META_LENGTH;
    
	ev->read_pt	= socks5_server_authorization_req_recv;
	return ev->read_pt( ev );
}

static status socks5_server_accept_callback_ssl( event_t * ev )
{
	connection_t * down = ev->data;

	if( !down->ssl->handshaked ) 
	{
		err(" downstream handshake error\n" );
		net_free( down );
		return ERROR;
	}
	timer_del( &ev->timer );

	down->recv 			= ssl_read;
	down->send 			= ssl_write;
	down->recv_chain 	= NULL;
	down->send_chain 	= ssl_write_chain;

	ev->read_pt 	= socks5_server_authorization_start;
	ev->write_pt 	= NULL;
	return ev->read_pt( ev );
}

static status socks5_server_accept_callback( event_t * ev )
{
	connection_t * down = ev->data;
	status rc = 0;

	do 
	{
		if( down->ssl_flag )
		{
			if( OK != ssl_create_connection( down, L_SSL_SERVER ) )
			{
				err("s5 server down ssl create connection failed\n");
				break;
			}
			rc = ssl_handshake( down->ssl );
			if( rc < 0 )
			{
				if( ERROR == rc )
				{
					err("s5 server down ssl handshake failed\n");
					break;
				}
				down->ssl->cb = socks5_server_accept_callback_ssl;
				timer_set_data( &ev->timer, down );
				timer_set_pt( &ev->timer, socks5_timeout_con );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			return socks5_server_accept_callback_ssl( ev );
		}
		ev->read_pt	= socks5_server_authorization_start;
		return ev->read_pt( ev );
	} while(0);
	
	net_free( down );
	return ERROR;
}

static status socks5_server_authorization_file_decode( meta_t * meta )
{
	ljson_ctx_t * ctx = NULL;
	ljson_node_t * root_obj, *db_arr, *db_index;
	ljson_node_t * username, *userpasswd;
	status rc = OK;

	if( OK != json_ctx_create( &ctx ) )
	{
		err("s5 server auth json ctx create\n");
		return ERROR;
	}
	if( OK !=  json_decode( ctx, meta->pos, meta->last ) )
	{
		err("s5 server auth json decode\n");
		rc = ERROR;
		goto failed;
	}
	 if( OK !=  json_get_child( &ctx->root, 1, &root_obj ) )
	 {
	 	err("s5 server auth json get root child\n");
		rc = ERROR;
		goto failed;
	 }
	 if( OK != json_get_obj_child_by_name( root_obj, "socks5_user_database", l_strlen("socks5_user_database"), &db_arr) )
	 {
		err("s5 server auth json get database arr\n");
		rc = ERROR;
		goto failed;
	 }
	 db_index = db_arr->child;
	 while( db_index )
	 {
		if( OK != json_get_obj_child_by_name( db_index, "username", l_strlen("username"), &username ) )
		{
			err("s5 server auth json array child find username\n");
			rc = ERROR;
			goto failed;
		}
		if( OK != json_get_obj_child_by_name( db_index, "passwd", l_strlen("passwd"), &userpasswd ) )
		{
			err("s5 server auth json array child find username\n");
			rc = ERROR;
			goto failed;
		}

        debug("s5 server auth info [%.*s]:[%.*s]\n", username->name.len, username->name.data, userpasswd->name.len, userpasswd->name.data );
		user_add( &username->name, &userpasswd->name );
		db_index = db_index->next;
	 }
failed:
	json_ctx_free( ctx );
	return rc;
}

static status socks5_server_authorization_file_load( meta_t * meta )
{
	int fd = 0;
	ssize_t size = 0;

	fd = open( conf.socks5.server.authfile, O_RDONLY  );
	if( ERROR == fd )
	{
		err("s5 server auth open file [%s] failed, errno [%d]\n", conf.socks5.server.authfile,  errno );
		return ERROR;
	}
	size = read( fd, meta->pos, meta_len( meta->start, meta->end ) );
	close( fd );
	if( size == ERROR )
	{
		err("s5 server auth read auth file failed\n");
		return ERROR;
	}
	meta->last += size;
	return OK;
}

static status socks5_server_authorization_init(  )
{
	meta_t * meta = NULL;
	status rc = ERROR;

	do 
	{
		if( strlen(conf.socks5.server.authfile) < 1 )
		{
			err("s5 server auth database filename error, [%s]\n", conf.socks5.server.authfile );
			break;
		}

		if( OK != meta_alloc( &meta, 4096 ) )
		{
			err("s5 server auth databse alloc meta failed\n");
			break;
		}

		if( OK != socks5_server_authorization_file_load( meta ) )
		{
			err("s5 server auth file load failed\n");
			break;
		}

		if( OK != socks5_server_authorization_file_decode( meta ) )
		{
			err("s5 server auth file decode failed\n");
			break;
		}
		rc = OK;
	}while(0);

	if( meta )
	{
		meta_free( meta );
	}
	return rc;
}

status socks5_init( void )
{
    int i = 0;
    if( this )
    {
        err("s5 init this not empty\n");
        return ERROR;
    }
    this = l_safe_malloc( sizeof(private_s5_t) + MAX_NET_CON*sizeof(socks5_cycle_t) );
    if( !this )
    {
        err("s5 init allo this failed, [%d]\n", this );
        return ERROR;
    }
    memset( this, 0, sizeof(private_s5_t) + MAX_NET_CON*sizeof(socks5_cycle_t) );
    queue_init(&this->usable);
    queue_init(&this->use);
    for( i = 0; i < MAX_NET_CON; i++ )
    {
        queue_insert_tail(&this->usable, &this->pool[i].queue );
    }
    return OK;
}

status socks5_server_init( void )
{
    if( SOCKS5_META_LENGTH < sizeof(socks5_auth_t) ||
        SOCKS5_META_LENGTH < sizeof(socks5_message_invite_t) ||
        SOCKS5_META_LENGTH < sizeof(socks5_message_advance_t) ||
        SOCKS5_META_LENGTH < 4096
       )
    {
        err("s5 macro SOCKS5_META_LENGTH too small\n");
        return ERROR;
    }
    
    if( OK != socks5_init() )
    {
        err("s5 server socks5 init failed\n");
        return ERROR;
    }
    
	if( conf.socks5_mode == SOCKS5_SERVER )
	{
		listen_add( conf.socks5.server.server_port, socks5_server_accept_callback, L_SSL );
		if( OK != socks5_server_authorization_init( ) )
        {
            err("s5 server authorization init failed\n");
        }
	}
	return OK;
}

status socks5_server_end( void )
{
	return OK;
}

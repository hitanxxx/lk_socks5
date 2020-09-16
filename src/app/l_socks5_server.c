#include "l_base.h"
#include "l_dns.h"
#include "l_socks5_local.h"
#include "l_socks5_server.h"


#if defined(S5_UNFIXED_CACHE)
static status socks5_remote2local_recv( event_t * ev );
static status socks5_local2remote_send( event_t * ev );
static status socks5_remote2local_send( event_t * ev );
static status socks5_local2remote_recv( event_t * ev );
#else
static status lks5_down_recv( event_t * ev );
static status lks5_up_send( event_t * ev );
static status lks5_up_recv( event_t * ev );
static status lks5_down_send( event_t * ev );
#endif

status socks5_cycle_free( socks5_cycle_t * cycle )
{
	if( cycle->up )
	{
		cycle->up->event.read_pt = NULL;
		cycle->up->event.write_pt = NULL;
	}
	if( cycle->down )
	{
		cycle->down->event.read_pt = NULL;
		cycle->down->event.write_pt = NULL;
	}

	if( cycle->up ) 
	{
		net_free( cycle->up );
		cycle->up = NULL;
	}
	if( cycle->down ) 
	{
		net_free( cycle->down );
		cycle->down = NULL;
	}
#if defined(S5_UNFIXED_CACHE)
	if( cycle->local2remote ) 
	{
		net_transport_free( cycle->local2remote );
		cycle->local2remote = NULL;
	}
	if( cycle->remote2local ) 
	{
		net_transport_free( cycle->remote2local );
		cycle->remote2local = NULL;
	}
#endif
	if( cycle->dns_cycle )
	{
		err("dns cycle free [%p]\n", cycle->dns_cycle );
		l_dns_free( cycle->dns_cycle );
		cycle->dns_cycle = NULL;
	}
	l_safe_free( cycle );
	return OK;
}

inline void socks5_timeout_cycle( void * data )
{
	socks5_cycle_free( (socks5_cycle_t *)data );
}

inline void socks5_timeout_con( void * data )
{
	net_free( (connection_t *)data );
}


#if defined(S5_UNFIXED_CACHE)
static status socks5_remote2local_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->remote2local, 0 );
	if( rc == ERROR ) {
		err("remote2local recv failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	
	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	return rc;
}

static status socks5_local2remote_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;
	
	rc = net_transport( cycle->local2remote, 1 );
	if( rc == ERROR ) {
		err("local2remote send failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	
	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	
	return rc;
}

static status socks5_remote2local_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->remote2local, 1 );
	if( rc == ERROR ) {
		err("remote2local send failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	
	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	
	return rc;
}

static status socks5_local2remote_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->local2remote, 0 );
	if( rc == ERROR ) {
		err("local2remote recv failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	
	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	return rc;
}
#else
static status lks5_down_recv( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	meta_t * meta = down->meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	if( cycle->down_recv_error != 1 )
	{
		while( meta_len( meta->last, meta->end ) > 0 )
		{
			rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
			if( rc == ERROR )
			{
				err("[%p] - down recv failed\n", cycle );
				cycle->down_recv_error = 1;
				break;
			}
			else if ( rc == AGAIN )
			{
				break;
			}
			meta->last += rc;
		}
	}
	
	if( meta_len( meta->pos, meta->last ) > 0 )
	{
		down->event.read_pt = NULL;
		cycle->up->event.write_pt = lks5_up_send;

		return cycle->up->event.write_pt( &cycle->up->event );
	}
	return rc;
}

/*
 * send always send the data that recvd, it will be
 * close the connection when send error.
 */
static status lks5_up_send( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	meta_t * meta = cycle->down->meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( meta_len( meta->pos, meta->last ) > 0 )
	{
		rc = up->send( up, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc == ERROR )
		{
			err("up send failed, close the socks5 cycle\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
		else if ( rc == AGAIN )
		{
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		meta->pos += rc;
	}

	if( cycle->down_recv_error == 1 )
	{
		err("[%p] - down->up remain empty, down recv error, close the socks5 cycle\n", cycle );
		socks5_cycle_free( cycle );
		return ERROR;
	}

	meta->pos = meta->last = meta->start;
	up->event.write_pt = NULL;
	cycle->down->event.read_pt = lks5_down_recv;

	return cycle->down->event.read_pt( &cycle->down->event );
}

static status lks5_up_recv( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	meta_t * meta = up->meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	if( cycle->up_recv_error != 1 )
	{
		while( meta_len( meta->last, meta->end ) > 0 )
		{
			rc = up->recv( up, meta->last, meta_len( meta->last, meta->end ) );
			if( rc == ERROR )
			{
				err("[%p] - up recv failed\n", cycle );
				cycle->up_recv_error = 1;
				break;
			}
			else if ( rc == AGAIN )
			{
				break;
			}
			meta->last += rc;
		}
	}
	
	if( meta_len( meta->pos, meta->last ) > 0 )
	{
		up->event.read_pt = NULL;
		cycle->down->event.write_pt = lks5_down_send;

		return cycle->down->event.write_pt( &cycle->down->event );
	}
	return rc;
}

static status lks5_down_send( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	meta_t * meta = cycle->up->meta;
	ssize_t rc = 0;

	timer_set_data( &ev->timer, (void*)cycle );
	timer_set_pt( &ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( meta_len( meta->pos, meta->last ) > 0 )
	{
		rc = down->send( down, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc == ERROR )
		{
			err("down send failed, close the socks5 cycle\n");
			socks5_cycle_free( cycle );
			return ERROR;
		}
		else if ( rc == AGAIN )
		{
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		meta->pos += rc;
	}

	if( cycle->up_recv_error == 1 )
	{
		err("[%p] - up->down remain empty, up recv error, close the socks5 cycle\n", cycle );
		socks5_cycle_free( cycle );
		return ERROR;
	}

	meta->pos = meta->last = meta->start;
	down->event.write_pt = NULL;
	cycle->up->event.read_pt = lks5_up_recv;

	return cycle->up->event.read_pt( &cycle->up->event );
}
#endif

status socks5_pipe( event_t * ev )
{
	connection_t * c = ev->data;
	socks5_cycle_t * cycle = c->data;
	l_meta_t * meta = NULL;
	
#if defined(S5_UNFIXED_CACHE)
	if( OK != net_transport_alloc( &cycle->local2remote ) ) {
		err(" net_transport local2remote alloc\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	if( OK != net_transport_alloc( &cycle->remote2local ) ) {
		err(" net_transport remote2local alloc\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}

	cycle->local2remote->recv_connection = cycle->down;
	cycle->local2remote->send_connection = cycle->up;

	cycle->remote2local->recv_connection = cycle->up;
	cycle->remote2local->send_connection = cycle->down;

	event_opt( &cycle->up->event, cycle->up->fd, EV_R );
	event_opt( &cycle->up->event, cycle->up->fd, EV_W );

	cycle->up->event.read_pt = socks5_remote2local_recv;
	cycle->up->event.write_pt = socks5_local2remote_send;

	event_opt( &cycle->down->event, cycle->down->fd, EV_R );	
	event_opt( &cycle->down->event, cycle->down->fd, EV_W );	
	
	cycle->down->event.read_pt = socks5_local2remote_recv;
	cycle->down->event.write_pt = socks5_remote2local_send;
#else
	cycle->down->event.read_pt = lks5_down_recv;
	cycle->down->event.write_pt = NULL;

	cycle->up->event.read_pt = lks5_up_recv;
	cycle->up->event.write_pt = NULL;

	event_opt( &cycle->up->event, cycle->up->fd, EV_W );
	event_opt( &cycle->up->event, cycle->up->fd, EV_R );

	event_opt( &cycle->down->event, cycle->down->fd, EV_W );
	event_opt( &cycle->down->event, cycle->down->fd, EV_R );

	// local will use this function, need to alloc down and up meta buffer
	if( cycle->down->meta == NULL )
	{
		
		if( OK != meta_alloc( &cycle->down->meta, SOCKS5_META_LENGTH ) )
		{
			err(" down meta alloc\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	meta = cycle->down->meta;
	meta->pos = meta->last = meta->start;

	if( cycle->up->meta == NULL )
	{
		if( OK != meta_alloc( &cycle->up->meta, SOCKS5_META_LENGTH ) )
		{
			err(" up meta alloc\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	meta = cycle->up->meta;
	meta->pos = meta->last = meta->start;
#endif
	return cycle->down->event.read_pt( &cycle->down->event );
}

static status socks5_server_s5_msg_advance_response_send( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	status rc;

	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) 
	{
		err(" send failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	else if( rc == DONE ) 
	{
		timer_del( &down->event.timer );
		debug ( "socks5 local response send success\n" );

		ev->write_pt 	= NULL;
		ev->read_pt 	= socks5_pipe;
		return ev->read_pt( ev );
	}
	
	timer_set_data( &down->event.timer, (void*)cycle );
	timer_set_pt( &down->event.timer, socks5_timeout_cycle );
	timer_add( &down->event.timer, SOCKS5_TIME_OUT );
	return rc;
}

static status socks5_server_s5_msg_advance_response_build( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	connection_t * down = cycle->down;
	socks5_message_advance_response_t * response = NULL;
	struct sockaddr addr;
	struct sockaddr_in *addr_in = NULL;
	socklen_t socklen = 0;

	down->meta->last = down->meta->pos = down->meta->start;
	response = ( socks5_message_advance_response_t* )down->meta->last;

	memset( &addr, 0, sizeof(struct sockaddr) );
	if( OK != getsockname( cycle->up->fd, &addr, &socklen ) )
	{
		err("get local address info failed\n");
		socks5_cycle_free( cycle );
		return ERROR;
	}
	addr_in = (struct sockaddr_in *)&addr;

	response->ver 	=	0x05;
	response->rep	=	0x00;
	response->rsv	=	0x00;
	response->atyp	=	0x01;
	response->bnd_addr	=	htons((uint16_t)addr_in->sin_addr.s_addr);
	response->bnd_port	=	htons(addr_in->sin_port);

	down->meta->last += sizeof(socks5_message_advance_response_t);

	up->event.read_pt 	= NULL;
	up->event.write_pt 	= NULL;

	down->event.write_pt = socks5_server_s5_msg_advance_response_send;
	return down->event.write_pt( &down->event );
}

static status socks5_server_connect_up_check( event_t * ev )
{
	connection_t* up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc;

	if( OK != l_socket_check_status( up->fd ) ) {
		err(" socks5 local connect failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	l_socket_nodelay( up->fd );
	timer_del( &ev->timer );
	debug("connect up check success\n" );

	ev->read_pt		= NULL;
	ev->write_pt 	= socks5_server_s5_msg_advance_response_build;
	return ev->write_pt( ev );
}

static status socks5_server_connect_up( event_t * ev )
{
	connection_t * up = ev->data;
	socks5_cycle_t * cycle = up->data;
	status rc;

	cycle->down->event.read_pt 	= NULL;
	cycle->down->event.write_pt = NULL;

	if( !cycle->up->meta ) 
	{
		if( OK != meta_alloc( &cycle->up->meta, SOCKS5_META_LENGTH ) ) 
		{
			err(" up meta alloc\n" );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}

	rc = l_net_connect( cycle->up, &cycle->up->addr, TYPE_TCP );
	if( ERROR == rc ) {
		err(" up net connect failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->event.read_pt 	= NULL;
	cycle->up->event.write_pt 	= socks5_server_connect_up_check;
	event_opt( &cycle->up->event, cycle->up->fd, EV_W);

	if( AGAIN == rc ) 
	{
		timer_set_data( &cycle->up->event.timer, (void*)cycle );
		timer_set_pt( &cycle->up->event.timer, socks5_timeout_cycle );
		timer_add( &cycle->up->event.timer, SOCKS5_TIME_OUT );
		return rc;
	}
	return cycle->up->event.write_pt( &cycle->up->event );
}

static status socks5_server_connection_try_read( event_t * ev  )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;

	char buf[1];
	socklen_t len;
	int err = 0;
	ssize_t n;

	len = sizeof(int);
	if( getsockopt( down->fd, SOL_SOCKET, SO_ERROR, (void*)&err, &len ) == -1 ) 
	{
		err = errno;
	}

	if( err )
	{
		err("socks5 down try read failed\n");
		socks5_cycle_free( cycle );
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
			
			cycle->up->event.read_pt = NULL;
			cycle->up->event.write_pt = socks5_server_connect_up;
			cycle->up->event.write_pt( &cycle->up->event );
		}
		else
		{
			err("socks5 dns resolv failed\n");
			socks5_cycle_free( cycle );
		}
	}
}



static status socks5_server_s5_msg_advance_req_addr( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	char ipstr[128] = {0}, portstr[128] = {0};
	status rc;

	if( OK != net_alloc( &cycle->up ) ) {
		err(" net alloc up\n" );
		socks5_cycle_free( cycle );
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

		cycle->up->addr.sin_family	= AF_INET;
		cycle->up->addr.sin_port 	= htons( local_port );
		cycle->up->addr.sin_addr.s_addr = inet_addr( ipstr );

		cycle->up->event.read_pt 	= NULL;
		cycle->up->event.write_pt 	= socks5_server_connect_up;
		return cycle->up->event.write_pt( &cycle->up->event );
	}
	else if ( cycle->advance.atyp == 0x03 )
	{
		down->event.read_pt 	= socks5_server_connection_try_read;
		down->event.write_pt 	= NULL;
		
		// domain type address
		rc = l_dns_create( &cycle->dns_cycle );
		if( rc == ERROR )
		{
			err("dns cycle create failed\n");
			socks5_cycle_free( cycle );
			return ERROR;
		}
		strncpy( cycle->dns_cycle->query, cycle->advance.addr_str, sizeof(cycle->dns_cycle->query) );
		strncpy( cycle->dns_cycle->dns_serv, l_dns_get_serv(), sizeof(cycle->dns_cycle->dns_serv) );
		cycle->dns_cycle->cb 		= socks5_server_s5_msg_advance_req_addr_dns_callback;
		cycle->dns_cycle->cb_data 	= cycle;

		return l_dns_start( cycle->dns_cycle );
	}
	err(" not support socks5 request atyp [%x]\n", cycle->advance.atyp );
	socks5_cycle_free( cycle );
	return ERROR;
}


static status socks5_server_s5_msg_advance_req_recv( event_t * ev )
{
	char * p = NULL;
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	ssize_t rc;
	int32 i = 0;

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
		if( down->meta->pos == down->meta->last )
		{
			rc = down->recv( down, down->meta->last, meta_len( down->meta->last, down->meta->end ) );
			if( rc < 0 )
			{
				if(  rc == AGAIN )
				{
					timer_set_data( &down->event.timer, (void*)cycle );
					timer_set_pt( &down->event.timer, socks5_timeout_cycle );
					timer_add( &down->event.timer, SOCKS5_TIME_OUT );
					return AGAIN;
				}
				err(" recv socks5 request failed, errno [%d]\n", errno );
				socks5_cycle_free( cycle );
				return ERROR;
			}
			down->meta->last += rc;
		}

		state = cycle->advance.state;
		for( ; down->meta->pos < down->meta->last; down->meta->pos ++ )
		{
			p = down->meta->pos;
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
				err("request atyp [%d] not support\n", cycle->advance.atyp );
				socks5_cycle_free( cycle );
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
					err("only support CMD `connect` 0x01, [%d]\n", *p );
					socks5_cycle_free( cycle );
					return ERROR;
				}
				if( cycle->advance.atyp == 0x04 )
				{
					err("not support ipv6 request, atype [%d]\n", cycle->advance.atyp );
					socks5_cycle_free( cycle );
					return ERROR;
				}
		
				debug(" socks5 request process success\n" );
				cycle->advance.state = 0;
				timer_del( &down->event.timer );

				ev->write_pt = NULL;
				ev->read_pt  = socks5_server_s5_msg_advance_req_addr;

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
	status rc;

	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) 
	{
		err(" send auth replay failed, [%d]\n", down->fd );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	else if ( rc == DONE ) 
	{
		timer_del( &ev->timer );

		down->meta->pos = down->meta->last = down->meta->start;

		ev->write_pt 	= NULL;
		ev->read_pt 	= socks5_server_s5_msg_advance_req_recv;
		return ev->read_pt( ev );
	}
	timer_set_data( &ev->timer, cycle );
	timer_set_pt(&ev->timer, socks5_timeout_cycle );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );

	return AGAIN;
}

static status socks5_server_s5_msg_invate_resp_build( event_t * ev )
{
	connection_t * down = ev->data;
	socsk5_message_invite_response_t * invite_resp = NULL;
	char * p = NULL;
	/*
		s5 msg invate resp format
		1 byte 		1 byte
		version | ack authentication method
	*/
	down->meta->pos = down->meta->last = down->meta->start;
	invite_resp = ( socsk5_message_invite_response_t* ) down->meta->pos;
	invite_resp->ver	 	= 0x05;
	invite_resp->method 	= 0x00;
	down->meta->last += sizeof(socsk5_message_invite_response_t);

	ev->read_pt 	= NULL;
	ev->write_pt 	= socks5_server_s5_msg_invate_resp_send;
	event_opt( ev, down->fd, EV_W );
	return ev->write_pt( ev );
}

static status socks5_server_s5_msg_invate_req_recv( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_cycle_t * cycle = down->data;
	ssize_t rc;
	char * p = NULL;
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
		if( down->meta->pos == down->meta->last )
		{
			rc = down->recv( down, down->meta->last, meta_len( down->meta->last, down->meta->end ) );
			if( rc < 0 )
			{
				if( rc == AGAIN )
				{
					timer_set_data( &down->event.timer, (void*)cycle );
					timer_set_pt( &down->event.timer, socks5_timeout_cycle );
					timer_add( &down->event.timer, SOCKS5_TIME_OUT );
					return AGAIN;
				}
				err(" recv socks5 auth failed\n" );
				socks5_cycle_free( cycle );
				return ERROR;
			}
			down->meta->last += rc;
		}

		for( ; down->meta->pos < down->meta->last; down->meta->pos ++ )
		{
			p = down->meta->pos;

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
					timer_del( &down->event.timer );

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
	socks5_cycle_t * cycle;
	status rc = 0;

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

	ev->write_pt 	= NULL;
	ev->read_pt  	= socks5_server_s5_msg_invate_req_recv;
	return ev->read_pt( ev );
}

static status socks5_server_authorization_resp_send( event_t * ev )
{
	connection_t * down = ev->data;
	status rc;

	rc = down->send_chain( down, down->meta );
	if( rc == ERROR )
	{
		err("socks5 server auth send resp failed\n");
		net_free( down );
		return ERROR;
	}
	else if ( rc == DONE )
	{
		timer_del( &ev->timer );

		ev->write_pt = socks5_server_cycle_init;
		return ev->write_pt( ev );
	}

	timer_set_data( &ev->timer, down );
	timer_set_pt(&ev->timer, socks5_timeout_con );
	timer_add( &ev->timer, SOCKS5_TIME_OUT );
	return AGAIN;
}

static status socks5_server_authorization_req_check( event_t * ev )
{
	connection_t * down = ev->data;
	socks5_auth_t * auth = NULL;
	status rc = SOCKS5_AUTH_SUCCESS;
	string_t auth_name, auth_passwd;
	user_t * user;

	auth = (socks5_auth_t*)down->meta->pos;
	if( auth->magic != SOCKS5_AUTH_MAGIC_NUM )
	{
		err("check magic [%d] check failed. magic [%d]\n", auth->magic, SOCKS5_AUTH_MAGIC_NUM );
		rc = SOCKS5_AUTH_MAGIC_FAIL;
		goto failed;
	}
	if( auth->message_type != SOCKS5_AUTH_REQ )
	{
		err("check auth type [%x] != SOCKS5_AUTH_REQ\n", auth->message_type );
		rc = SOCKS5_AUTH_TYPE_FAIL;
		goto failed;
	}

	memset( &auth_name, 0, sizeof(string_t) );
	memset( &auth_passwd, 0, sizeof(string_t) );

	auth_name.data = auth->name;
	auth_name.len = l_strlen(auth->name);

	if( OK != user_find( &auth_name, &user ) )
	{
		err("can't find user [%.*s]\n", auth_name.len, auth_name.data );
		rc = SOCKS5_AUTH_NO_USER;
		goto failed;
	}

	auth_passwd.data = auth->passwd;
	auth_passwd.len = l_strlen(auth->passwd);

	if(  l_strlen(user->passwd) != auth_passwd.len || strncmp( user->passwd, auth_passwd.data, auth_passwd.len ) != 0 )
	{
		err("user [%.*s] auth passwd not right\n", auth_name.len, auth_name.data );
		rc = SOCKS5_AUTH_PASSWD_FAIL;
		goto failed;
	}

failed:
	auth->message_type       =  SOCKS5_AUTH_RESP;
	auth->message_status     =  rc;

	ev->read_pt 	= NULL;
	ev->write_pt 	= socks5_server_authorization_resp_send;
	event_opt( &down->event, down->fd, EV_W );
	
	return ev->write_pt( ev );
}

static status socks5_server_authorization_req_recv( event_t * ev )
{
	connection_t * down = ev->data;
	meta_t * meta = down->meta;
	ssize_t rc;

	while( meta_len( meta->pos, meta->last ) < sizeof(socks5_auth_t) )
	{
		rc = down->recv( down, meta->last, meta_len( meta->last, meta->end ) );
		if( rc < 0 )
		{
			if( rc == AGAIN )
			{
				timer_set_data( &ev->timer, down );
				timer_set_pt(&ev->timer, socks5_timeout_con );
				timer_add( &ev->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err("socks5 server auth recv failed\n");
			net_free(down);
			return ERROR;
		}
		down->meta->last += rc;
	}
	timer_del( &ev->timer );

	debug("recv private length [%d], socks5_auth_t size [%d]\n", meta_len( meta->pos, meta->last ), sizeof(socks5_auth_t) );
	ev->read_pt = socks5_server_authorization_req_check;
	return ev->read_pt( ev );
}

static status socks5_server_authorization_start( event_t * ev )
{
	connection_t * down = ev->data;

	if( !down->meta )
	{
		if( OK != meta_alloc( &down->meta, SOCKS5_META_LENGTH ) )
		{
			err("down meta alloc\n" );
			net_free( down );
			return ERROR;
		}
	}
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

	down->recv = ssl_read;
	down->send = ssl_write;
	down->recv_chain = NULL;
	down->send_chain = ssl_write_chain;

	ev->read_pt 	= socks5_server_authorization_start;
	ev->write_pt 	= NULL;

	return ev->read_pt( ev );
}

static status socks5_server_accept_callback( event_t * ev )
{
	connection_t * down = ev->data;
	status rc;

	if( down->ssl_flag )
	{
		if( OK != ssl_create_connection( down, L_SSL_SERVER ) )
		{
			err(" downstream ssl create\n" );
			goto failed;
		}

		rc = ssl_handshake( down->ssl );
		if( rc < 0 )
		{
			if( rc == AGAIN )
			{
				down->ssl->cb = socks5_server_accept_callback_ssl;
				timer_set_data( &down->event.timer, (void*)down );
				timer_set_pt( &down->event.timer, socks5_timeout_con );
				timer_add( &down->event.timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err( " downstream ssl handshake\n" );
			goto failed;
		}
		return socks5_server_accept_callback_ssl( ev );
	}
	ev->read_pt = socks5_server_authorization_start;
	return ev->read_pt( ev );
failed:
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
		err("ctx create\n");
		return ERROR;
	}
	if( OK !=  json_decode( ctx, meta->pos, meta->last ) )
	{
		err("json decode\n");
		rc = ERROR;
		goto failed;
	}
	 if( OK !=  json_get_child( &ctx->root, 1, &root_obj ) )
	 {
	 	err("json get root child\n");
		rc = ERROR;
		goto failed;
	 }
	 if( OK != json_get_obj_child_by_name( root_obj, "socks5_user_database", l_strlen("socks5_user_database"), &db_arr) )
	 {
		err("get database arr\n");
		rc = ERROR;
		goto failed;
	 }
	 db_index = db_arr->child;
	 while( db_index )
	 {
		if( OK != json_get_obj_child_by_name( db_index, "username", l_strlen("username"), &username ) )
		{
			err("array child find username\n");
			rc = ERROR;
			goto failed;
		}
		if( OK != json_get_obj_child_by_name( db_index, "passwd", l_strlen("passwd"), &userpasswd ) )
		{
			err("array child find username\n");
			rc = ERROR;
			goto failed;
		}

		debug("username [%.*s]  --- passwd [%.*s]\n",
			username->name.len, username->name.data,
			userpasswd->name.len, userpasswd->name.data );

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
		err("open auth file failed, errno [%d]\n", errno );
		return ERROR;
	}
	size = read( fd, meta->pos, meta_len( meta->start, meta->end ) );
	close( fd );
	if( size == ERROR )
	{
		err("read auth file\n");
		return ERROR;
	}
	meta->last += size;
	return OK;
}

static status socks5_server_authorization_init(  )
{
	meta_t * meta;
	status rc = OK;

	if( strlen(conf.socks5.server.authfile) < 1 )
	{
		err("auth file path null\n");
		return ERROR;
	}
	if( OK != meta_alloc( &meta, 4096 ) )
	{
		err("meta alloc failed\n");
		return ERROR;
	}
	if( OK != socks5_server_authorization_file_load( meta ) )
	{
		err("load user file failed\n");
		rc = ERROR;
		goto failed;
	}
	if( OK != socks5_server_authorization_file_decode( meta ) )
	{
		err("auth decode\n");
		rc = ERROR;
		goto failed;
	}
failed:
	meta_free( meta );
	return rc;
}

status socks5_server_init( void )
{
	if( conf.socks5_mode == SOCKS5_SERVER )
	{
		listen_add( conf.socks5.server.server_port, socks5_server_accept_callback, L_SSL );
		socks5_server_authorization_init( );
	}
	return OK;
}

status socks5_server_end( void )
{
	return OK;
}

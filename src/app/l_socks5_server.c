#include "lk.h"

status socks5_cycle_free( socks5_cycle_t * cycle )
{
	if( cycle->up) {
		net_free( cycle->up );
		cycle->up = NULL;
	}
	if( cycle->down) {
		net_free( cycle->down );
		cycle->down = NULL;
	}
	if( cycle->local2remote ) {
		net_transport_free( cycle->local2remote );
		cycle->local2remote = NULL;
	}
	if( cycle->remote2local ) {
		net_transport_free( cycle->remote2local );
		cycle->remote2local = NULL;
	}
	l_safe_free( cycle );
	return OK;
}

void socks5_timeout_cycle( void * data )
{
	socks5_cycle_t * cycle;

	cycle = data;
	socks5_cycle_free( cycle );
}

void socks5_timeout_con( void * data )
{
	connection_t * c;
	c = data;

	net_free( c );
}

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

status socks5_pipe( event_t * event )
{
	connection_t * c;
	socks5_cycle_t * cycle;

	c = event->data;
	cycle = c->data;
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
	
	return AGAIN;
}

static status socks5_local_response( event_t * event )
{
	connection_t * down;
	socks5_cycle_t * cycle;

	down = event->data;
	cycle = down->data;
	status rc;
	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) {
		err(" send failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if( rc == DONE ) {
		timer_del( &down->event.timer );
		debug ( " socks5 local response send success\n" );

		event->write_pt = NULL;
		event->read_pt = socks5_pipe;
		return event->read_pt( event );
	}
	timer_set_data( &down->event.timer, (void*)cycle );
	timer_set_pt( &down->event.timer, socks5_timeout_cycle );
	timer_add( &down->event.timer, SOCKS5_TIME_OUT );
	return rc;
}

static status socks5_server_response_prepare( event_t * event )
{
	socks5_cycle_t * cycle;
	connection_t* up, *down;

	up = event->data;
	cycle = up->data;
	down = cycle->down;
	
	down->meta->last = down->meta->pos = down->meta->start;
	// ver
	*down->meta->last++ = 0x05;
	// rep
	*down->meta->last++ = 0x00;
	// rsv
	*down->meta->last++ = 0x00;
	// atyp
	*down->meta->last++ = 0x01;
	// BND ADDR
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	// BND PORT
	*down->meta->last++ = 0x10;
	*down->meta->last++ = 0x10;
	
	cycle->up->event.read_pt = NULL;
	cycle->up->event.write_pt = NULL;
	
	down->event.write_pt = socks5_local_response;
	return down->event.write_pt( &down->event );
}


static status socks5_server_connect_test( connection_t * c )
{
	int	err = 0;
    socklen_t  len = sizeof(int);

	if (getsockopt( c->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) {
		err = errno;
	}
	if (err) {
		err(" connect test failed, %d\n", errno );
		return ERROR;
	}
	return OK;
}

static status socks5_server_connect_check( event_t * event )
{
	connection_t* c;
	status rc;
	socks5_cycle_t * cycle;

	c = event->data;
	cycle = c->data;
	if( OK != l_socket_check_status( c->fd ) ) {
		err(" socks5 local connect failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	l_socket_nodelay( c->fd );
	timer_del( &event->timer );
	debug(" server remote check success\n" );
	
	event->write_pt = socks5_server_response_prepare;
	return event->write_pt( event );
}


static status socks5_server_connect( event_t * event )
{
	connection_t * down;
	socks5_cycle_t * cycle;
	char ipstr[128] = {0}, portstr[128] = {0};
	string_t ip, port;
	status rc;
	
	down = event->data;
	cycle = down->data;
	
	if( cycle->request.atyp == 0x01 ) {
		// ip type address
		snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d", (unsigned char )cycle->request.dst_addr[0],
		(unsigned char )cycle->request.dst_addr[1],
		(unsigned char )cycle->request.dst_addr[2],
		(unsigned char )cycle->request.dst_addr[3]
		);
		snprintf( portstr, sizeof(portstr), "%d", ntohs(*(int32*)cycle->request.dst_port) );
		ip.data = ipstr;
		ip.len = l_strlen(ipstr);
		port.data = portstr;
		port.len = l_strlen(portstr);
	} else if ( cycle->request.atyp == 0x03 ) {
		// domain type address
		
		ip.data = cycle->request.dst_addr;
		ip.len = (unsigned char)cycle->request.host_len;
		snprintf( portstr, sizeof(portstr), "%d", ntohs(*(int32*)cycle->request.dst_port) );
		port.data = portstr;
		port.len = l_strlen(portstr);		
	} else {
		err(" not support socks5 request atyp [%x]\n", cycle->request.atyp );
	}

	if( OK != net_alloc( &cycle->up ) ) {
		err(" net alloc up\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->data = cycle;

	event->read_pt = NULL;
	event->write_pt = NULL;
	
	rc = l_net_connect( cycle->up, &ip, &port );
	if( ERROR == rc ) {
		err(" up net connect failed\n" );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	event_opt( &cycle->up->event, cycle->up->fd, EV_W);

	cycle->up->event.read_pt = NULL;
	cycle->up->event.write_pt = socks5_server_connect_check;
	
	if( AGAIN == rc ) {
		timer_set_data( &cycle->up->event.timer, (void*)cycle );
		timer_set_pt( &cycle->up->event.timer, socks5_timeout_cycle );
		timer_add( &cycle->up->event.timer, SOCKS5_TIME_OUT );
		return rc;
	}
	return cycle->up->event.write_pt( &cycle->up->event );
}

static status socks5_process_request( event_t * event )
{
	char * p = NULL;
	connection_t * c;
	socks5_cycle_t * cycle = NULL;
	ssize_t rc;
	int32 i = 0;

	enum{
		ver = 0,
		cmd,
		rsv,
		atyp,
		dst_addr,
		dst_host,
		dst_port,
		dst_port_end
	} state;

	c = event->data;
	cycle = c->data;
	
	while( 1 ) {
		if( c->meta->pos == c->meta->last ) {
			rc = c->recv( c, c->meta->last, meta_len( c->meta->last, c->meta->end ) );			
			if( rc < 0 ) {
				if(  rc == AGAIN ) {
					c->event.timer.data = (void*)cycle;
					c->event.timer.timeout_handler = socks5_timeout_cycle;
					timer_add( &c->event.timer, SOCKS5_TIME_OUT );
					return AGAIN;
				}
				err(" recv socks5 request failed, errno [%d]\n", errno );
				socks5_cycle_free( cycle );
				return ERROR;
			}
			c->meta->last += rc;
		}

		state = cycle->request.state;
		for( ; c->meta->pos < c->meta->last; c->meta->pos ++ ) {
			p = c->meta->pos;

			if( state == ver ) {
				cycle->request.ver = *p;
				state = cmd;
				continue;
			}
			if( state == cmd ) {
				cycle->request.cmd = *p;
				if( cycle->request.cmd != 0x01 ) {
					err("only support CMD `connect` 0x01, [%d]\n", *p );
					socks5_cycle_free( cycle );
					return ERROR;
				}
				state = rsv;
				continue;
			}
			if( state == rsv ) {
				cycle->request.rsv = *p;
				state = atyp;
				continue;
			}
			if( state == atyp ) {
				cycle->request.atyp = *p;
				state = dst_addr;
				continue;
			}
			if( state == dst_addr ) {
				cycle->request.dst_addr[cycle->request.offset++] = *p;
				if( cycle->request.atyp == 0x01 ) {
					if( cycle->request.offset == 4 ) {
						state = dst_port;
						continue;
					}
				} else if ( cycle->request.atyp == 0x03 ) {
					cycle->request.host_len = *p;
					cycle->request.offset = 0;
					state = dst_host;
					continue;
				} else if ( cycle->request.atyp == 0x04 ) {
					err("not support ipv6 now\n" );
					socks5_cycle_free( cycle );
					return ERROR;
				}
			}
			if( state == dst_host ) {
				cycle->request.dst_addr[cycle->request.offset++] = *p;
				if( cycle->request.offset == (unsigned char)cycle->request.host_len ) {
					state = dst_port;
					continue;
				}
			}
			if( state == dst_port ) {
				cycle->request.dst_port[0] = *p;
				state = dst_port_end;
				continue;
			}
			if( state == dst_port_end ) {
				cycle->request.dst_port[1] = *p;
				
				debug(" socks5 request process success\n" );
				cycle->request.state = 0;
				timer_del( &c->event.timer );
	
				event->write_pt = NULL;
				event->read_pt = socks5_server_connect;
				return event->read_pt( event );
			}
		}
		cycle->request.state = state;
	}
}

static status socks5_server_auth_response ( event_t * event )
{
	connection_t * down;
	status rc;
	socks5_cycle_t * cycle = NULL;
	
	down = event->data;
	cycle = down->data;
	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) {
		err(" send auth replay failed, [%d]\n", down->fd );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if ( rc == DONE ) {
		timer_del( &event->timer );

		down->meta->pos = down->meta->last = down->meta->start;

		event->write_pt = NULL;
		event->read_pt = socks5_process_request;
		return event->read_pt( event );
	}
	timer_set_data( &event->timer, cycle );
	timer_set_pt(&event->timer, socks5_timeout_cycle );
	timer_add( &event->timer, SOCKS5_TIME_OUT );

	return AGAIN;
}

static status socks5_server_auth_response_prepare( event_t * event )
{
	connection_t * c;
	
	c = event->data;
	char * p = NULL;
	/*
		1 byte 		1 byte
		version | ack authentication method
	*/
	c->meta->pos = c->meta->last = c->meta->start;
	*c->meta->last ++ = 0x05;
	*c->meta->last ++ = 0x00;

	for( p = c->meta->pos; p < c->meta->last; p ++ ) {
		debug(" socks5 auth replay [%x]\n", *p );
	}
	
	event_opt( event, c->fd, EV_W );
	
	event->read_pt = NULL;
	event->write_pt = socks5_server_auth_response;
	return event->write_pt( event );
}

static status socks5_server_auth_parse( event_t * event )
{
	connection_t * down;
	socks5_cycle_t * cycle;
	ssize_t rc;
	char * p = NULL;
	/*
		1 byte		1 byte				1-255 byte
		version | authentication method num | auth methods
	*/
	enum {
		ver = 0,
		nmethod,
		methods
	} state;
	
	down = event->data;
	cycle = down->data;
	
	state = cycle->init.state;
	while( 1 ) {
		
		if( down->meta->pos == down->meta->last ) {
			rc = down->recv( down, down->meta->last, meta_len( down->meta->last, down->meta->end ) );
			if( rc < 0 ) {
				if( rc == AGAIN ) {
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
		
		for( ; down->meta->pos < down->meta->last; down->meta->pos ++ ) {
			p = down->meta->pos;
			
			if( state == ver ) {
				cycle->init.ver = *p;
				state = nmethod;
				continue;
			}
			if( state == nmethod ) {
				cycle->init.nmethod = *p;
				cycle->init.m_offset = 0;
				state = methods;
				continue;
			}
			if( state == methods ) {
				cycle->init.method[(int)cycle->init.m_offset++] = *p;
				if( cycle->init.m_offset == cycle->init.nmethod ) {
					// recv auth over
					timer_del( &down->event.timer );
					
					event->read_pt = socks5_server_auth_response_prepare;
					return event->read_pt(event);
				}
			}
		}
		cycle->init.state = state;
	}
}

static status socks5_server_init_cycle( event_t * event )
{
	connection_t * c;
	socks5_cycle_t * cycle = NULL;

	c = event->data;
	if( !c->meta ) {
		if( OK != meta_alloc( &c->meta, 4096 ) ) {
			err(" down meta alloc\n" );
			net_free( c );
			return ERROR;
		}
	}
	cycle = l_safe_malloc( sizeof(socks5_cycle_t) );
	if( !cycle ) {
		err(" malloc socks5 cycle failed\n" );
		net_free( c );
		return ERROR;
	}
	memset( cycle, 0, sizeof(socks5_cycle_t) );
	cycle->down = c;
	c->data = (void*)cycle;
	
	event->read_pt = socks5_server_auth_parse;
	return event->read_pt( event );
}

static status socks5_server_accept_ssl_handshake( event_t * event )
{
	connection_t * c;
	
	c = event->data;
	
	if( !c->ssl->handshaked ) {
		err(" downstream handshake error\n" );
		net_free( c );
		return ERROR;
	}
	timer_del( &event->timer );
	
	c->recv = ssl_read;
	c->send = ssl_write;
	c->recv_chain = NULL;
	c->send_chain = ssl_write_chain;

	event->read_pt = socks5_server_init_cycle;
	event->write_pt = NULL;
	return event->read_pt( event );
}

static status socks5_server_accept_check( event_t * event )
{
	connection_t * c;
	status rc;

	c = event->data;

	if( c->ssl_flag ) {
		if( OK != ssl_create_connection( c, L_SSL_SERVER ) ) {
			err(" downstream ssl create\n" );
			goto failed;
		}
		
		rc = ssl_handshake( c->ssl );
		if( rc < 0 ) {
			if( rc == AGAIN ) {
				c->ssl->handler = socks5_server_accept_ssl_handshake;
				timer_set_data( &c->event.timer, (void*)c );
				timer_set_pt( &c->event.timer, socks5_timeout_con );
				timer_add( &c->event.timer, SOCKS5_TIME_OUT );
				return AGAIN;
			}
			err( "%s -- downstream ssl handshake\n" );
			goto failed;
		}
		return socks5_server_accept_ssl_handshake( event );
	}
	
	event->read_pt = socks5_server_init_cycle;
	return event->read_pt( event );
failed:
	net_free( c );
	return ERROR;
}


status socks5_server_init( void )
{
	if( conf.socks5_mode == SOCKS5_SERVER ) {
		listen_add( conf.socks5_server_port, socks5_server_accept_check, L_SSL );
	}
	return OK;
}

status socks5_server_end( void )
{
	return OK;
}

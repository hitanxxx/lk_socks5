#include "lk.h"

// socks5_cycle_free ---------
status socks5_cycle_free( socks5_cycle_t * cycle )
{
	if( cycle->up ) {
		net_free( cycle->up );
		cycle->up = NULL;
	}
	if( cycle->down ) {
		net_free( cycle->down );
		cycle->down = NULL;
	}
	if( cycle->in ) {
		net_transport_free( cycle->in );
	}
	if( cycle->out ) {
		net_transport_free( cycle->out );
	}
	l_safe_free( cycle );
	return OK;
}
// socks5_cycle_time_out --------
void socks5_cycle_time_out( void * data )
{
	socks5_cycle_t * cycle;

	cycle = data;
	socks5_cycle_free( cycle );
}
// socks5_connection_time_out ------
void socks5_connection_time_out( void * data )
{
	connection_t * c;
	
	c = data;
	net_free( c );
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
// socks5_transport_out_recv ------
static status socks5_transport_out_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->out, 0 );
	if( rc == ERROR ) {
		err_log("%s --- net transport out recv failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->read->timer.data = (void*)cycle;
	cycle->up->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->up->read->timer, SOCKS5_TIME_OUT );
	return rc;
}
// socks5_transport_out_send ------
static status socks5_transport_out_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->out, 1 );
	if( rc == ERROR ) {
		err_log("%s --- net transport out send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->up->read->timer.data = (void*)cycle;
	cycle->up->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->up->read->timer, SOCKS5_TIME_OUT );
	return rc;
}
// socks5_transport_in_recv ------
static status socks5_transport_in_recv( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->in, 0 );
	if( rc == ERROR ) {
		err_log("%s --- net transport in recv failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->down->read->timer.data = (void*)cycle;
	cycle->down->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->down->read->timer, SOCKS5_TIME_OUT );
	return rc;
}
// socks5_transport_in_send ------
static status socks5_transport_in_send( event_t * ev )
{
	connection_t * c;
	status rc;
	socks5_cycle_t * cycle;

	c = ev->data;
	cycle = c->data;

	rc = net_transport( cycle->in, 1 );
	if( rc == ERROR ) {
		err_log("%s --- net transport in send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	cycle->down->read->timer.data = (void*)cycle;
	cycle->down->read->timer.handler = socks5_cycle_time_out;
	timer_add( &cycle->down->read->timer, SOCKS5_TIME_OUT );
	return OK;
}
// socks5_pipe -------
status socks5_pipe( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;

	down = ev->data;
	cycle = down->data;
	if( OK != net_transport_alloc( &cycle->in ) ) {
		err_log("%s --- net_transport in alloc", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}
	if( OK != net_transport_alloc( &cycle->out ) ) {
		err_log("%s --- net_transport out alloc", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	}

	cycle->in->recv_connection = cycle->down;
	cycle->in->send_connection = cycle->up;

	cycle->out->recv_connection = cycle->up;
	cycle->out->send_connection = cycle->down;

	cycle->down->read->handler = socks5_transport_in_recv;
	cycle->up->write->handler = socks5_transport_in_send;

	cycle->down->write->handler = socks5_transport_out_send;
	cycle->up->read->handler = socks5_transport_out_recv;

	event_opt( cycle->up->read, EVENT_READ );
	event_opt( cycle->down->write, EVENT_WRITE );
	event_opt( cycle->down->read, EVENT_READ );
	event_opt( cycle->up->write, EVENT_WRITE );

	return AGAIN;
}
// socks5_local_response ----
static status socks5_local_response( event_t * ev )
{
	connection_t * down;
	socks5_cycle_t * cycle;

	down = ev->data;
	cycle = down->data;
	status rc;
	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) {
		err_log( "%s ---  send failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if( rc == DONE ) {
		timer_del( &down->write->timer );
		debug_log ( "%s --- socks5 local response send success", __func__ );
		down->read->handler = socks5_pipe;
		return down->read->handler( down->read );
	}
	down->write->timer.data = (void*)cycle;
	down->write->timer.handler = socks5_cycle_time_out;
	timer_add( &down->write->timer, SOCKS5_TIME_OUT );
	return rc;
}
// socks5_server_response_prepare -----
static status socks5_server_response_prepare( event_t * ev )
{
	socks5_cycle_t * cycle;
	connection_t* up, *down;

	up = ev->data;
	cycle = up->data;
	down = cycle->down;

	down->meta->last = down->meta->pos = down->meta->start;
	*down->meta->last++ = 0x05;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x01;

	//
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;
	*down->meta->last++ = 0x00;

	*down->meta->last++ = 0x10;
	*down->meta->last++ = 0x10;
	
	up->read->handler = NULL;
	up->write->handler = NULL;
	event_opt( down->write, EVENT_WRITE );
	down->write->handler = socks5_local_response;
	return down->write->handler( down->write );
}
// socks5_server_connect_check ----
static status socks5_server_connect_check( event_t * ev )
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

	up->write->handler = socks5_server_response_prepare;
	return up->write->handler( up->write );
}
// socks5_server_connect_start ------
static status socks5_server_connect_start( event_t * ev )
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
	up->write->handler = socks5_server_connect_check;
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
// socks5_server_address_domain -------
static status socks5_server_address_domain( socks5_cycle_t * cycle ) 
{
	string_t ip, port;
	struct addrinfo * res = NULL;
	char portstr[20] = {0};
	
	debug_log("%s --- domain [%.*s]", __func__, 
		(unsigned char)cycle->request.host_len,
		cycle->request.dst_addr
	);
	
	ip.data = cycle->request.dst_addr;
	ip.len = (unsigned char)cycle->request.host_len;
	snprintf( portstr, sizeof(portstr), "%d", ntohs(*(int32*)cycle->request.dst_port) );
	port.data = portstr;
	port.len = l_strlen(portstr);
	debug_log("%s --- port [%.*s]", __func__, port.len, port.data );
	
	res = net_get_addr( &ip, &port );
	if( !res ) {
		err_log("%s --- get up address failed", __func__ );
		return ERROR;
	}
	memset( &cycle->up->addr, 0, sizeof(struct sockaddr_in) );
	memcpy( &cycle->up->addr, res->ai_addr, sizeof(struct sockaddr_in) );
	freeaddrinfo( res );
	return OK;
}
// socks5_server_address_ipv4 ---------
static status socks5_server_address_ipv4( socks5_cycle_t * cycle )
{
	struct addrinfo * res = NULL;
	string_t ip, port;
	char ipstr[100] = {0}, portstr[20] = {0};
	
	// getaddrinfo
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
	debug_log("%s --- ip [%.*s] port [%.*s]", __func__ ,
	ip.len, ip.data,
	port.len, port.data
	);
	res = net_get_addr( &ip, &port );
	if( !res ) {
		err_log("%s --- get up address failed", __func__ );
		return ERROR;
	}
	memset( &cycle->up->addr, 0, sizeof(struct sockaddr_in) );
	memcpy( &cycle->up->addr, res->ai_addr, sizeof(struct sockaddr_in) );
	freeaddrinfo( res );
	return OK;
}
// socks5_server_connect -----------
static status socks5_server_connect( event_t * ev )
{
	connection_t * c, * up;
	socks5_cycle_t * cycle;
	
	c = ev->data;
	cycle = c->data;
	debug_log("%s --- socks5_server_connect", __func__  );
	if( OK != net_alloc( &cycle->up ) ) {
		err_log("%s --- net alloc up", __func__ );
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
			err_log( "%s --- up meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}

	if( cycle->request.atyp == 0x01 ) {
		if( OK != socks5_server_address_ipv4( cycle ) ) {
			err_log("%s --- server address ipv4 failed", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	} else if ( cycle->request.atyp == 0x03 ) {
		if( OK != socks5_server_address_domain( cycle ) ) {
			err_log("%s --- server address ipv4 failed", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	} else {
		err_log("%s --- not support socks5 request atyp [%x]", __func__, cycle->request.atyp );
	}

	cycle->down->read->handler = NULL;
	cycle->down->write->handler = NULL;

	cycle->up->read->handler = NULL;
	cycle->up->write->handler = socks5_server_connect_start;

	return cycle->up->write->handler( cycle->up->write );
}
// socks5_process_request -------------
static status socks5_process_request( event_t * ev )
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

	c = ev->data;
	cycle = c->data;

	while( 1 ) {
		if( c->meta->pos == c->meta->last ) {
			rc = c->recv( c, c->meta->last, meta_len( c->meta->last, c->meta->end ) );
			if( rc == ERROR ) {
				err_log("%s --- recv socks5 request failed", __func__ );
				socks5_cycle_free( cycle );
				return ERROR;
			} else if ( rc == AGAIN ) {
				c->read->timer.data = (void*)cycle;
				c->read->timer.handler = socks5_cycle_time_out;
				timer_add( &c->read->timer, SOCKS5_TIME_OUT );
				return AGAIN;
			} else {
				c->meta->last += rc;
			}
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
					if( cycle->request.offset == 16 ) {
						state = dst_port;
						continue;
					}
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

				// debug_log("%s --- process request success", __func__ );
				// debug_log("%s --- version [%x]", __func__, cycle->request.ver );
				// debug_log("%s --- cmd [%x]", __func__, cycle->request.cmd );
				// debug_log("%s --- rsv [%x]", __func__, cycle->request.rsv );
				// debug_log("%s --- atyp[%x]", __func__, cycle->request.atyp);
				
				// debug_log("%s --- dst addr [%d %d %d %d]", __func__,
					// (unsigned char)cycle->request.dst_addr[0],
					// (unsigned char)cycle->request.dst_addr[1],
					// (unsigned char)cycle->request.dst_addr[2],
					// (unsigned char)cycle->request.dst_addr[3]
				// );

				// debug_log("%s --- dst port [%d]", __func__, ntohs(*(int32*)cycle->request.dst_port ));
	
				debug_log("%s --- socks5 request process success", __func__ );
				cycle->request.state = 0;
				c->read->handler = socks5_server_connect;
				return c->read->handler( c->read );
			}
		}
		cycle->request.state = state;
	}
}
// socks5_server_auth_response ------------
static status socks5_server_auth_response ( event_t * ev )
{
	connection_t * down;
	status rc;
	socks5_cycle_t * cycle = NULL;

	down = ev->data;
	cycle = down->data;
	rc = down->send_chain( down, down->meta );
	if( rc == ERROR ) {
		err_log("%s --- send auth replay failed", __func__ );
		socks5_cycle_free( cycle );
		return ERROR;
	} else if ( rc == DONE ) {
		debug_log("%s --- send auth response success", __func__ );
		timer_del( &down->read->timer );

		down->meta->pos = down->meta->last = down->meta->start;

		down->write->handler = NULL;
		down->read->handler = socks5_process_request;
		return down->read->handler( down->read );
	}
	down->read->timer.data = (void*)cycle;
	down->read->timer.handler = socks5_cycle_time_out;
	timer_add( &down->read->timer, SOCKS5_TIME_OUT );
	return AGAIN;
}
// socks5_server_auth_response_prepare ----------
static status socks5_server_auth_response_prepare( event_t * ev )
{
	connection_t * c;
	c = ev->data;
	char * p = NULL;
	/*
		1 byte 		1 byte
		version | ack authentication method
	*/
	c->meta->pos = c->meta->last = c->meta->start;
	*c->meta->last ++ = 0x05;
	*c->meta->last ++ = 0x00;

	debug_log("%s --- socks5 replay len [%d]", __func__, meta_len( c->meta->pos, c->meta->last ) );
	for( p = c->meta->pos; p < c->meta->last; p ++ ) {
		debug_log("%s --- socks5 replay [%x]", __func__, *p );
	}

	c->read->handler = NULL;
	c->write->handler = socks5_server_auth_response;
	event_opt( c->write, EVENT_WRITE );

	return c->write->handler( c->write );
}
// socks5_local_auth_process ----------
static status socks5_local_auth_process( event_t * ev )
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
		method
	} state;
	
	down = ev->data;
	cycle = down->data;
	
	while( 1 ) {
		state = cycle->init.state;
		if( down->meta->pos == down->meta->last ) {
			rc = down->recv( down, down->meta->last, meta_len( down->meta->last, down->meta->end ) );
			if( rc == ERROR ) {
				err_log("%s --- recv socks5 auth failed", __func__ );
				socks5_cycle_free( cycle );
				return ERROR;
			} else if ( rc == AGAIN ) {
				debug_log("%s --- recv socks5 again", __func__ );
				return AGAIN;
			} else {
				down->meta->last += rc;
			}
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
				state = method;
				continue;
			}
			if( state == method ) {
				cycle->init.method[cycle->init.offset++] = *p;
				if( cycle->init.offset == cycle->init.nmethod ) {
					debug_log("%s --- socks5 auth process success", __func__ );
					down->read->handler = socks5_server_auth_response_prepare;
					return down->read->handler( down->read );
				}
			}
		}
		cycle->init.state = state;
	}
}
// socks5_server_init_connection -----------
static status socks5_server_init_connection( event_t * ev )
{
	connection_t * c;
	socks5_cycle_t * cycle = NULL;

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
			err_log( "%s --- c meta alloc", __func__ );
			socks5_cycle_free( cycle );
			return ERROR;
		}
	}
	// c->send = sends;
	// c->recv = recvs;
	// c->send_chain = send_chains;
	// c->recv_chain = NULL;
	
	c->read->handler = socks5_local_auth_process;
	return c->read->handler( c->read );
}
// socks5_server_handshake ---------
static status socks5_server_handshake( event_t * ev )
{
	connection_t * down;

	down = ev->data;
	if( !down->ssl->handshaked ) {
		err_log( "%s --- downstream handshake error", __func__ );
		net_free( down );
		return ERROR;
	}
	timer_del( &down->read->timer );
	down->recv = ssl_read;
	down->send = ssl_write;
	down->recv_chain = NULL;
	down->send_chain = ssl_write_chain;

	down->write->handler = NULL;
	down->read->handler = socks5_server_init_connection;
	return down->read->handler( down->read );
}
// socks5_server_start -------
static status socks5_server_start( event_t * ev )
{
	connection_t* down;
	status rc;
	
	down = ev->data;
	if( OK != ssl_create_connection( down, L_SSL_SERVER ) ) {
		err_log( "%s --- downstream ssl create", __func__ );
		net_free( down );
		return ERROR;
	}
	rc = ssl_handshake( down->ssl );
	if( rc == ERROR ) {
		err_log( "%s -- downstream ssl handshake", __func__ );
		net_free( down );
		return ERROR;
	} else if ( rc == AGAIN ) {
		down->ssl->handler = socks5_server_handshake;
		down->read->timer.data = (void*)down;
		down->read->timer.handler = socks5_connection_time_out;
		timer_add( &down->read->timer, SOCKS5_TIME_OUT );
		return AGAIN;
	}
	return socks5_server_handshake( down->read );
}
// socks5_server_init -----------
status socks5_server_init( void )
{
	if( conf.socks5_mode == SOCKS5_SERVER ) {
		listen_add( 3333, socks5_server_start, HTTPS );
	}
	return OK;
}
// socks5_server_end -----------
status socks5_server_end( void )
{
	return OK;
}

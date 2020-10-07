#include "l_base.h"

typedef struct private_net
{
    queue_t         usable;
    queue_t         use;
    connection_t    pool[0];
} private_net_t;
static private_net_t * this = NULL;

status l_socket_nonblocking( int32 fd )
{
	int32 nb = 1;
	return ioctl( fd, FIONBIO, &nb );
}

status l_socket_reuseaddr( int32 fd )
{	
	int tcp_reuseaddr = 1;
	if( ERROR == setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&tcp_reuseaddr, sizeof(int)) ) 
	{
		err("net set socket reuseaddr failed, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

status l_socket_fastopen( int32 fd )
{
#if(0)
	/*
	some arm platforms do not support this feature
    */
	int  tcp_fastopen = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN, (const void *) &tcp_fastopen, sizeof(tcp_fastopen)) ) {
		err(" fastopen failed, [%d]\n", errno );
		return ERROR;
	}
#endif
	return OK;
}

status l_socket_nodelay(  int32 fd )
{
	int tcp_nodelay = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &tcp_nodelay, sizeof(int)) ) 
	{
		err("net set socket nodelay failed, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

status l_socket_nopush( int32 fd )
{
#if(0)
    int tcp_cork = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_CORK, (const void *) &tcp_cork, sizeof(int)) ) 
	{
		err("net set socket nopush failed, [%d]\n", errno );
		return ERROR;
	}
#endif
	return OK;
}

status l_socket_send_lowat( int32 fd )
{
#if(0)
	int lowat = 0;
	if( ERROR == setsockopt( fd, SOL_SOCKET, SO_SNDLOWAT, (const void*)&lowat, sizeof(int) ) )
	{
		err("net set socket lowat failed, [%d]\n", errno );
		return ERROR;
	}
#endif
	return OK;
}

status l_socket_check_status( int32 fd )
{
	int	err = 0;
    socklen_t  len = sizeof(int);

	if (getsockopt( fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) 
	{
		err = errno;
	}
	if (err) 
	{
		err(" socket get a error, %d\n", errno );
		return ERROR;
	}
	return OK;
}



status l_net_connect( connection_t * c, struct sockaddr_in * addr, uint32 con_type )
{
	status rc = 0;
    
    // copy s5 server addr to connection's addr
	if( 0 != memcmp( &c->addr, addr, sizeof(struct sockaddr_in) ) )
	{
		memcpy( &c->addr, addr, sizeof(struct sockaddr_in) );
	}
    c->con_type = con_type;
	if( c->con_type != TYPE_TCP && c->con_type != TYPE_UDP )
	{
		err("net connect not support connection type [%x]\n", con_type );
		return ERROR;
	}
	c->fd = socket(AF_INET, (c->con_type == TYPE_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0 );
	if( -1 == c->fd )
	{
		err("net connect open socket failed, [%d]\n", errno );
		return ERROR;
	}
	if( OK != l_socket_reuseaddr( c->fd ) )
	{
		err("net connect net socket reuseaddr failed\n" );
		return ERROR;
	}

	if( OK != l_socket_nonblocking( c->fd ) )
    {
		err("net connect set socket nonblock failed\n" );
		return ERROR;
	}

	do
	{
		rc = connect( c->fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in) );
		if( 0 != rc )
		{
			if( (errno == EAGAIN) || (errno == EALREADY) || (errno == EINPROGRESS) )
			{
				rc = AGAIN;
				break;
			}
			else if( errno == EINTR )
			{
				continue;
			}
			err("net connect failed, [%d]\n", errno );
			return ERROR;
		}
	} while(0);

	if( con_type == TYPE_TCP )
	{
		c->send 		= sends;
		c->recv 		= recvs;
		c->send_chain 	= send_chains;
		c->recv_chain 	= NULL;
	}
	return rc;	
}

status l_net_accept( event_t * ev )
{
	listen_t * listen = ev->data;
	int32 cfd;
	connection_t * c;
	struct sockaddr_in client_addr;	
	socklen_t len = sizeof( struct sockaddr_in );

	while( 1 ) 
	{
		memset( &client_addr, 0, len );
		cfd = accept( listen->fd, (struct sockaddr *)&client_addr, &len );
		if( -1 == cfd ) 
		{
			if( errno == EWOULDBLOCK || errno == EAGAIN ) 
			{
				return AGAIN;
			}
			err("net accept  failed, [%d]\n", errno );
			return ERROR;
		}
		if( ERROR == net_alloc( &c ) ) 
		{
			err("net accept alloc client connection faield\n");
			close( cfd );
			return ERROR;
		}
		memcpy( &c->addr, &client_addr, len );
		if( OK != l_socket_nonblocking( cfd ) ) 
		{
			err("net accept set socket nonblock failed\n" );
			net_free( c );
			return ERROR;
		}
		if( OK != l_socket_fastopen( cfd ) )
		{
			err("net accept set socket fastopen failed\n");
			net_free(c);
			return ERROR;
		}
		if( OK != l_socket_nodelay( cfd ) )
		{
			err("net accept set socket nodelay failed\n");
			net_free(c);
			return ERROR;
		}
		if( OK != l_socket_send_lowat( cfd ) )
		{
			err("net accept set socket lowat 0 failed\n");
			net_free(c);
			return ERROR;
		}
		
		c->fd 			= cfd;
		c->con_type 	= TYPE_TCP;
		c->recv 		= recvs;
		c->send 		= sends;
		c->recv_chain 	= NULL;
		c->send_chain 	= send_chains;
		c->ssl_flag 	= (listen->type == L_SSL ) ? 1 : 0;

		c->event->read_pt 	= listen->handler;
		c->event->write_pt 	= NULL;
		event_opt( c->event, c->fd, EV_R );
		c->event->read_pt( c->event );
	}
	return OK;
}


static status net_free_simple( event_t * ev )
{
	connection_t * c = ev->data;
	meta_t * cur = NULL, * next = NULL;

	event_free( ev );
	c->event = NULL;

	if( c->fd ) 
	{
		close( c->fd );
		c->fd = 0;
	}	
	if( c->meta )
	{
		cur = c->meta;
		while( cur )
		{
			next = cur->next;
			meta_free(cur);
			cur = next;
		}
		c->meta = NULL;
	}

	memset( &c->addr, 0, sizeof(struct sockaddr_in) );
	c->data 		= NULL;
	c->ssl 			= NULL;
	c->ssl_flag 	= 0;
	
	c->send			= NULL;
	c->send_chain	= NULL;
	c->recv			= NULL;
	c->recv_chain	= NULL;

	queue_remove( &c->queue );
	queue_insert_tail( &this->usable, &c->queue );
	return OK;
}

status net_free( connection_t * c )
{
	if( c == NULL )
	{
		return ERROR;
	}

	if( c->ssl_flag && c->ssl ) 
	{
		if( AGAIN == ssl_shutdown( c->ssl ) )
		{
			c->ssl->cb = net_free_simple;
			return AGAIN;
		}
	}
	c->event->write_pt  = NULL;
	c->event->read_pt   = net_free_simple;
	return c->event->read_pt( c->event );
}

status net_alloc( connection_t ** c )
{
	connection_t * new = NULL;
	queue_t * q = NULL;

	if( queue_empty( &this->usable ) )
	{
		err("net alloc usbale empty\n" );
		return ERROR;
	}

	q = queue_head( &this->usable );
	queue_remove( q );
	queue_insert_tail( &this->use, q );
	new = l_get_struct( q, connection_t, queue );

	if( NULL == new->event )
	{
		if( OK != event_alloc( &new->event ) )
		{
			err("net alloc conn event failed\n");
			queue_remove( q );
			queue_insert_tail( &this->usable, q );
			return ERROR;
		}
		new->event->data = new;
	}
	*c = new;
	return OK;
}

status net_init( void )
{
	uint32 i;

    if( this )
    {
        err("net init this not empty\n");
        return ERROR;
    }
    
    this = l_safe_malloc(sizeof(private_net_t)+(MAX_NET_CON*sizeof(connection_t)));
    if( !this )
    {
        err("net init malloc pirvate failed, [%d]\n", errno );
        return ERROR;
    }
    memset( this, 0, sizeof(private_net_t)+(MAX_NET_CON*sizeof(connection_t)) );
    
	queue_init( &this->usable );
	queue_init( &this->use );
	for( i = 0; i < MAX_NET_CON; i ++ ) 
	{
		queue_insert_tail( &this->usable, &this->pool[i].queue );
	}
	return OK;
}

status net_end( void )
{
	uint32 i;
	meta_t *cur = NULL, *next = NULL;
	
	for( i = 0; i < MAX_NET_CON; i ++ ) 
	{
		if( this->pool[i].meta )
		{
			cur = this->pool[i].meta;
			while( cur )
			{
				next = cur->next;
				meta_free( cur );
				cur = next;
			}
		}
	}
	if( this )
    {
		l_safe_free( this );
	}
	return OK;
}

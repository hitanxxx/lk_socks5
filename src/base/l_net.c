#include "l_base.h"

static queue_t usable;
static queue_t use;
static connection_t * pool = NULL;

status l_socket_nonblocking( int32 fd )
{
	int32 nb;
	nb = 1;
	return ioctl( fd, FIONBIO, &nb );
}

status l_socket_reuseaddr( int32 fd )
{	
	int tcp_reuseaddr = 1;
	if( ERROR == setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&tcp_reuseaddr, sizeof(tcp_reuseaddr)) ) {
		err(" reuseaddr failed, [%d]\n", errno );
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
	int  tcp_nodelay = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &tcp_nodelay, sizeof(tcp_nodelay)) ) {
		err(" nodelay failed, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

status l_socket_nopush( int32 fd )
{
    int  tcp_cork = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_CORK, (const void *) &tcp_cork, sizeof(tcp_cork)) ) {
		err(" nopush failed, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

status l_socket_check_status( int fd )
{
	int	err = 0;
    socklen_t  len = sizeof(int);

	if (getsockopt( fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) {
		err = errno;
	}
	if (err) 
	{
		err(" socket get a error, %d\n", errno );
		return ERROR;
	}
	return OK;
}

static status net_free_right_now( event_t * ev )
{
	connection_t * c = ev->data;
	status rc;
	meta_t * cur = NULL, * next = NULL;

	ev->read_pt 	= NULL;
	ev->write_pt 	= NULL;

	queue_remove( &c->queue );
	queue_insert_tail( &usable, &c->queue );

	if( c->fd ) 
	{
		close( c->fd );
		c->fd = 0;
	}
	if( c->event.f_active == 1 )
	{
		c->event.f_active = 0;
	}

	c->data = NULL;
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

	c->ssl = NULL;
	c->ssl_flag = 0;

	timer_del( &c->event.timer );
	memset( &c->event, 0, sizeof( event_t ) );

	c->send			= NULL;
	c->send_chain	= NULL;
	c->recv			= NULL;
	c->recv_chain	= NULL;
	
	c->event.data = (void*)c;
	return OK;
}

status net_free( connection_t * c )
{
	status rc;

	if( c == NULL )
	{
		return ERROR;
	}

	if( c->ssl_flag && c->ssl ) 
	{
		rc = ssl_shutdown( c->ssl );
		if( rc == AGAIN ) 
		{
			c->ssl->cb = net_free_right_now;
			return AGAIN;
		}
	}
	return net_free_right_now( &c->event );
}

status net_alloc( connection_t ** c )
{
	connection_t * new;
	queue_t * q;

	if( queue_empty( &usable ) ) 
	{
		err("usbale empty\n" );
		return ERROR;
	}

	q = queue_head( &usable );
	queue_remove( q );
	queue_insert_tail( &use, q );
	new = l_get_struct( q, connection_t, queue );
	*c = new;
	return OK;
}

status net_init( void )
{
	uint32 i;

	queue_init( &usable );
	queue_init( &use );
	
	pool = ( connection_t *) l_safe_malloc ( sizeof(connection_t) * MAXCON );
	if( !pool ) 
	{
		err(" l_safe_malloc pool\n" );
		return ERROR;
	}
	memset( pool, 0, sizeof(connection_t) * MAXCON );
	for( i = 0; i < MAXCON; i ++ ) 
	{
		pool[i].event.data = (void*)&pool[i];
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}

status net_end( void )
{
	uint32 i;

	for( i = 0; i < MAXCON; i ++ ) {
		if( pool[i].meta ) {
			meta_free( pool[i].meta );
			pool[i].meta = NULL;
		}
	}
	if( pool ) {
		l_safe_free( pool );
	}
	return OK;
}

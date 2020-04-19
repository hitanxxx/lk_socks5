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
	int  tcp_fastopen = 1;
	if( ERROR == setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN, (const void *) &tcp_fastopen, sizeof(tcp_fastopen)) ) {
		err(" fastopen failed, [%d]\n", errno );
		return ERROR;
	}
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
	if (err) {
		err(" socket get a error, %d\n", errno );
		return ERROR;
	}
	return OK;
}


struct addrinfo * net_get_addr( string_t * ip, string_t * port )
{
	struct addrinfo hints, *res;
	int rc;
	char name[100] = {0};
	char serv[10] = {0};

	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	memcpy( name, ip->data, ip->len );
	memcpy( serv, port->data, port->len );

	rc = getaddrinfo( name, serv, &hints, &res );
	if( 0 != rc ) {
		err(" getaddrinfo failed, [%d]\n", errno );
		return NULL;
	}
	if( NULL == res ) {
		err(" getaddrinfo failed, [%d]\n", errno );
		return NULL;
	}
	return res;
}

static status net_free_right_now( event_t * ev )
{
	connection_t * c;
	status rc;
	meta_t * cl, *n;

	c = ev->data;

	queue_remove( &c->queue );
	queue_insert_tail( &usable, &c->queue );

	if( c->fd ) {
		c->event.f_active = 0;
		close( c->fd );
		c->fd = 0;
	}
	
	c->data = NULL;
	if( c->meta ) {
		c->meta->pos = c->meta->last = c->meta->start;
		cl = c->meta->next;
		while( cl ) {
			debug(" free meta list loop\n" );
			n = cl->next;
			meta_free( cl );
			cl = n;
		}
	}
	memset( &c->addr, 0, sizeof(struct sockaddr_in) );
	c->active_flag = 0;

	c->ssl = NULL;
	c->ssl_flag = 0;

	timer_del( &c->event.timer );

	memset( &c->event, 0, sizeof( event_t ) );
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

	if( c->ssl_flag && c->ssl ) {
		rc = ssl_shutdown( c->ssl );
		if( rc == AGAIN ) {
			c->ssl->handler = net_free_right_now;
			return AGAIN;
		}
	}
	return net_free_right_now( &c->event );
}

status net_alloc( connection_t ** c )
{
	connection_t * new;
	queue_t * q;

	if( queue_empty( &usable ) ) {
		err(" usbale empty\n" );
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
	if( !pool ) {
		err(" l_safe_malloc pool\n" );
		return ERROR;
	}
	memset( pool, 0, sizeof(connection_t) * MAXCON );
	for( i = 0; i < MAXCON; i ++ ) {
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

#include "lk.h"

static queue_t usable;
static queue_t use;
static connection_t * pool = NULL;
static event_t * read_pool = NULL;
static event_t * write_pool = NULL;

// net_non_blocking ---------------
status net_non_blocking( int fd )
{
	int32 nb;
	nb = 1;
	return ioctl( fd, FIONBIO, &nb );
}
// net_fastopen ------------
status net_fastopen( connection_t * c )
{
	int  tcp_fastopen = 1;
	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_FASTOPEN,
			(const void *) &tcp_fastopen, sizeof(tcp_fastopen)) ) {
		err_log( "%s --- fastopen failed, [%d]", __func__, errno );
		return ERROR;
	}
	return OK;
}
// net_nodelay -------------
status net_nodelay( connection_t * c )
{
	int  tcp_nodelay = 1;
	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_NODELAY,
			(const void *) &tcp_nodelay, sizeof(tcp_nodelay)) ) {
		err_log( "%s --- nodelay failed, [%d]", __func__, errno );
		return ERROR;
	}
	return OK;
}
// net_nopush ---------------------
status net_nopush( connection_t * c )
{
    int  tcp_cork = 1;
	if( ERROR == setsockopt(c->fd, IPPROTO_TCP, TCP_CORK,
			(const void *) &tcp_cork, sizeof(tcp_cork)) ) {
		err_log( "%s --- nopush failed, [%d]", __func__, errno );
		return ERROR;
	}
	return OK;
}
// net_get_addr ------------------
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
		err_log( "%s --- getaddrinfo failed, [%d]", __func__, errno );
		return NULL;
	}
	if( NULL == res ) {
		err_log( "%s --- getaddrinfo failed, [%d]", __func__, errno );
		return NULL;
	}
	return res;
}
// net_free ---------------
status net_free( connection_t * c )
{
	meta_t * cl, *n;

	queue_remove( &c->queue );
	queue_insert_tail( &usable, &c->queue );

	if( c->fd ) {
		event_close( c->read, EVENT_READ );
		event_close( c->write, EVENT_WRITE );
		close( c->fd );
		c->fd = 0;
	}
	c->data = NULL;
	if( c->meta ) {
		c->meta->pos = c->meta->last = c->meta->start;
		cl = c->meta->next;
		while( cl ) {
			n = cl->next;
			meta_free( cl );
			cl = n;
		}
	}
	memset( &c->addr, 0, sizeof(struct sockaddr_in) );
	c->active_flag = 0;

	c->ssl = NULL;
	c->ssl_flag = 0;

	timer_del( &c->read->timer );
	c->read->handler = NULL;
	c->read->f_active = 0;
	timer_del( &c->write->timer );
	c->write->handler = NULL;
	c->write->f_active = 0;
	return OK;
}
// net_alloc --------------
status net_alloc( connection_t ** c )
{
	connection_t * new;
	queue_t * q;

	if( queue_empty( &usable ) ) {
		err_log( "%s --- usbale empty", __func__ );
		return ERROR;
	}

	q = queue_head( &usable );
	queue_remove( q );
	queue_insert_tail( &use, q );
	new = l_get_struct( q, connection_t, queue );
	*c = new;
	return OK;
}
// net_init --------------
status net_init( void )
{
	uint32 i;

	queue_init( &usable );
	queue_init( &use );
	read_pool = (event_t*)l_safe_malloc( sizeof(event_t)* MAXCON );
	if( !read_pool ) {
		err_log( "%s --- l_safe_malloc read pool", __func__ );
		return ERROR;
	}
	memset( read_pool, 0, sizeof(event_t)*MAXCON );

	write_pool = (event_t*)l_safe_malloc( sizeof(event_t)*MAXCON );
	if( !write_pool ) {
		err_log( "%s --- l_safe_malloc write pool", __func__ );
		return ERROR;
	}
	memset( write_pool, 0, sizeof(event_t)*MAXCON );

	pool = ( connection_t *) l_safe_malloc ( sizeof(connection_t) * MAXCON );
	if( !pool ) {
		err_log( "%s --- l_safe_malloc pool", __func__ );
		return ERROR;
	}
	memset( pool, 0, sizeof(connection_t) * MAXCON );
	for( i = 0; i < MAXCON; i ++ ) {
		pool[i].read = &read_pool[i];
		pool[i].write = &write_pool[i];
		pool[i].read->data = (void*)&pool[i];
		pool[i].write->data = (void*)&pool[i];
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}
// net_end ---------------
status net_end( void )
{
	uint32 i;
	if( read_pool ){
		l_safe_free( read_pool );
	}
	if( write_pool ) {
		l_safe_free( write_pool );
	}

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

#include "lk.h"

mem_list_t * listens = NULL;

// listen_add ----------------
status listen_add( uint32 port, listen_pt handler, uint32 type )
{
	listen_t *  listen;

	listen = mem_list_push( listens );
	if( !listen ) {
		err_log( "%s --- list push", __func__ );
		return ERROR;
	}
	listen->handler = handler;
	listen->port = port;
	listen->type = type;
	listen->c = NULL;
	if( conf.reuse_port && process_num > 1 ) {
		listen->reuse_port = 1;
	}
	mem_list_create( &listen->list, sizeof(listen_t) );
	return OK;
}
// listen_open -----------------
static status listen_open( listen_t * listening )
{
	int	reuseaddr = 1;
	int reuseport = 1;
	int tcp_nodelay = 1;
	int tcp_fastopen = 1;

	listening->server_addr.sin_family = AF_INET;
	listening->server_addr.sin_port = htons( listening->port );
	listening->server_addr.sin_addr.s_addr = htonl( INADDR_ANY );
	listening->fd = socket( AF_INET, SOCK_STREAM, 0 );
	if( ERROR == listening->fd ) {
		err_log( "%s --- socket failed, [%d]", __func__, errno );
		return ERROR;
	}
	if( OK != net_non_blocking( listening->fd ) ) {
		close( listening->fd );
		err_log( "%s --- nonblock failed, [%d]", __func__, errno );
		return ERROR;
	}
	if( ERROR == setsockopt( listening->fd, SOL_SOCKET, SO_REUSEADDR,
		(const void *)&reuseaddr, sizeof(reuseaddr)) ) {
		close( listening->fd );
		err_log( "%s --- reuseaddr failed, [%d]", __func__, errno );
		return ERROR;
	}
	if( conf.reuse_port ) {
		if( listening->reuse_port ) {
			if( ERROR == setsockopt( listening->fd, SOL_SOCKET, SO_REUSEPORT,
				(const void *)&reuseport, sizeof(reuseport)) ) {
				err_log( "%s --- reuseport failed, [%d]", __func__, errno );
				close( listening->fd );
				return ERROR;
			}
		}
	}
	if( ERROR == setsockopt( listening->fd, IPPROTO_TCP, TCP_FASTOPEN,
			(const void *) &tcp_fastopen, sizeof(tcp_fastopen)) ) {
		err_log( "%s --- fastopen failed, [%d]", __func__, errno );
		return ERROR;
	}
	if( ERROR == setsockopt( listening->fd, IPPROTO_TCP, TCP_NODELAY,
			(const void *) &tcp_nodelay, sizeof(tcp_nodelay)) ) {
		err_log( "%s --- nodelay failed, [%d]", __func__, errno );
		return ERROR;
	}
	if( OK != bind( listening->fd, (struct sockaddr *)&listening->server_addr,
		sizeof(struct sockaddr) ) ) {
		err_log( "%s --- bind failed, [%d]", __func__, errno );
		close( listening->fd );
		return ERROR;
	}
	if( OK != listen( listening->fd, 500 ) ) {
		err_log( "%s --- listen failed, [%d]", __func__, errno );
		close( listening->fd );
		return ERROR;
	}
	return OK;
}
// listen_close -----------------
static status listen_close( listen_t * listening )
{
	close( listening->fd );
	listening->error = 1;
	return OK;
}

// listen_start ------------------
status listen_start( void )
{
	uint32 i, j;
	listen_t * listen_head, *listen;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_head = mem_list_get( listens, i+1 );
		if( OK != listen_open( listen_head ) ) {
			err_log("%s --- listen_open failed, [%d]", __func__, errno );
			listen_head->error = 1;
			return ERROR;
		}
		if( listen_head->reuse_port ) {
			listen_close( listen_head );
			for( j = 0; j < process_num; j ++ ) {
				listen = mem_list_push( listen_head->list );
				listen->handler = listen_head->handler;
				listen->port = listen_head->port;
				listen->type = listen_head->type;
				listen->c = NULL;
				listen->reuse_port = 1;
				if( OK != listen_open( listen ) ) {
					err_log("%s -- listen_open failed, [%d]", __func__, errno );
					listen->error = 1;
					return ERROR;
				}
			}
		}
	}
	return OK;
}
// listen_stop -----------
status listen_stop( void )
{
	uint32 i, j;
	listen_t *listen_head, *listen;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_head = mem_list_get( listens, i+1 );
		if( listen_head->error ) continue;
		for( j = 0; j < listen_head->list->elem_num; j ++ ) {
			listen = mem_list_get( listen_head->list, j + 1 );
			if( listen->error ) continue;
			listen_close( listen );
		}
		listen_close( listen_head );
	}
	return OK;
}
// listen_init --------------------
status listen_init( void )
{
	if( OK != mem_list_create( &listens, sizeof(listen_t) ) ) {
		err_log( "%s --- listens list", __func__ );
		return ERROR;
	}
	return OK;
}
// listen_end --------------------
status listen_end( void )
{
	uint32 i;
	listen_t 	*listen_head;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_head = mem_list_get( listens, i+1 );
		mem_list_free( listen_head->list );
	}
	mem_list_free( listens );
	return OK;
}

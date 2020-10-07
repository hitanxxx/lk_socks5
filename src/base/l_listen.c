#include "l_base.h"

mem_list_t * listens = NULL;

status listen_add( uint32 port, listen_pt handler, uint32 type )
{
	listen_t *  listen;

	listen = mem_list_push( listens );
	if( !listen ) {
		err("listen add list push\n");
		return ERROR;
	}
	listen->handler = handler;
	listen->port = port;
	listen->type = type;
	return OK;
}

static status listen_open( listen_t * listens )
{
	listens->server_addr.sin_family 		= AF_INET;
	listens->server_addr.sin_port 			= htons( (uint16_t)listens->port );
	listens->server_addr.sin_addr.s_addr 	= htonl( INADDR_ANY );

	do 
	{
		listens->fd = socket( AF_INET, SOCK_STREAM, 0 );
		if( -1 == listens->fd ) 
		{
			err("listen open listen socket failed\n");
			break;
		}
		if( OK != l_socket_nonblocking( listens->fd ) ) 
		{
			err("listen set socket non blocking failed\n");
			break;
		}
		if( OK != l_socket_reuseaddr( listens->fd ) )
		{
			err("listen set socket reuseaddr failed\n" );
			break;
		}
		if( OK != l_socket_fastopen( listens->fd ) )
		{
			err("listen set socket fastopen failed\n" );
			break;
		}
		if( OK != l_socket_nodelay( listens->fd ) )
		{
			err("listen set socket nodelay failed\n" );
			break;
		}
		if( OK != bind( listens->fd, (struct sockaddr *)&listens->server_addr, sizeof(struct sockaddr) ) )
		{
			err("listen bind failed, [%d]\n", errno );
			break;
		}
		if( OK != listen( listens->fd, 100 ) )
		{
			err("listen call listen failed\n" );
			break;
		}
		return OK;
	}while(0);

	return ERROR;
}

status listen_stop( void )
{
	uint32 i;
	listen_t *listen;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen = mem_list_get( listens, i+1 );
		if( listen->fd ) {
			close( listen->fd );
			listen->fd = 0;
		}
	}
	return OK;
}

status listen_start( void )
{
	uint32 i;
	listen_t * listen;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen = mem_list_get( listens, i+1 );
		
		if( OK != listen_open( listen ) ) {
			err("listen open failed\n");
			goto failed;
		}
	}
	return OK;
failed:
	listen_stop( );
	return ERROR;
}

status listen_init( void )
{
	if( OK != mem_list_create( &listens, sizeof(listen_t) ) ) 
	{
		err("listens list create failed\n" );
		return ERROR;
	}
	return OK;
}

status listen_end( void )
{
	if( listens ) 
	{
		mem_list_free( listens );
		listens = NULL;
	}
	return OK;
}

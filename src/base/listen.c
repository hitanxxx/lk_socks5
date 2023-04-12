#include "common.h"
#include "dns.h"
#include "s5_server.h"
#include "s5_local.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"

mem_arr_t * listens = NULL;

static status listen_add( uint32 port, listen_pt handler, uint32 type )
{
	listen_t *  listen;

	listen = mem_arr_push( listens );
	if( !listen ) 
	{
		err("listen add list push\n");
		return ERROR;
	}
	listen->handler = handler;
	listen->port 	= port;
	listen->type 	= type;
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
		if( OK != net_socket_nbio( listens->fd ) ) 
		{
			err("listen set socket non blocking failed\n");
			break;
		}
		if( OK != net_socket_resueaddr( listens->fd ) )
		{
			err("listen set socket reuseaddr failed\n" );
			break;
		}
		if( OK != net_socket_fastopen( listens->fd ) )
		{
			err("listen set socket fastopen failed\n" );
			break;
		}
		if( OK != net_socket_nodelay( listens->fd ) )
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

	for( i = 0; i < listens->elem_num; i ++ ) 
	{
		listen = mem_arr_get( listens, i+1 );
		if( listen->fd > 0 ) 
		{
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

	for( i = 0; i < listens->elem_num; i ++ ) 
	{
		listen = mem_arr_get( listens, i+1 );
#if !defined(EVENT_EPOLL)
		listen->event.idx = i;
#endif
		if( OK != listen_open( listen ) )
		{
			err("listen open failed\n");
			goto failed;
		}
	}
	return OK;
failed:
	listen_stop( );
	return ERROR;
}


int listen_num( )
{
	if( listens ) return listens->elem_num;
	return 0;
}

status listen_init( void )
{	
    int i = 0;
    if( OK != mem_arr_create( &listens, sizeof(listen_t) ) ) 
    {
        err("listens list create failed\n" );
        return ERROR;
    }

    // s5 local listen
    if( config_get()->s5_mode == SOCKS5_CLIENT )
    {
        listen_add( config_get()->s5_local_port, socks5_local_accept_cb, L_NOSSL );
    }
    // s5 server listen
    if( config_get()->s5_mode == SOCKS5_SERVER )
    {
        listen_add( config_get()->s5_serv_port, socks5_server_accept_cb, L_SSL );
    }
    // webserver listen
    for( i = 0; i < config_get()->http_num; i ++ )
    {
        listen_add( config_get()->http_arr[i], webser_accept_cb, L_NOSSL );
    }
    // webserver ssl listen
    for( i = 0; i < config_get()->https_num; i ++ )
    {
        listen_add( config_get()->https_arr[i], webser_accept_cb_ssl, L_SSL );
    }

    if( OK != listen_start() )
    {
    err("listen start failed\n");
    return ERROR;
    }
    return OK;
}

status listen_end( void )
{
	if( listens ) 
	{
		mem_arr_free( listens );
		listens = NULL;
	}
	return OK;
}

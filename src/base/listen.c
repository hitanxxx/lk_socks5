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
	listen_t *  p = mem_arr_push( listens );
	if( !p )  {
		err("listen arr push failed\n");
		return ERROR;
	}
	p->handler = handler;
	p->port = port;
	p->type = type;
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
		if( -1 == listens->fd )  {
			err("listen open listen socket failed\n");
			break;
		}
		if( OK != net_socket_nbio( listens->fd ) )  {
			err("listen set socket non blocking failed\n");
			break;
		}
		if( OK != net_socket_reuseaddr( listens->fd ) )	{
			err("listen set socket reuseaddr failed\n" );
			break;
		}
		if( OK != net_socket_reuseport( listens->fd ) )	{
			err("listen set socket reuseport failed\n" );
			break;
		}
		if( OK != net_socket_fastopen( listens->fd ) ) {
			err("listen set socket fastopen failed\n" );
			break;
		}
		if( OK != net_socket_nodelay( listens->fd ) ){
			err("listen set socket nodelay failed\n" );
			break;
		}
		if( OK != bind( listens->fd, (struct sockaddr *)&listens->server_addr, sizeof(struct sockaddr) ) ) {
			err("listen bind failed, [%d]\n", errno );
			break;
		}
		if( OK != listen( listens->fd, 100 ) ) {
			err("listen call listen failed\n" );
			break;
		}
		return OK;
	}while(0);

	return ERROR;
}

status listen_stop( void )
{
	for( int i = 0; i < listens->elem_num; i ++ )  {
		listen_t * p = mem_arr_get( listens, i+1 );
		if( p->fd > 0 )  {
			close( p->fd );
			p->fd = 0;
		}
	}
	return OK;
}

status listen_start( void )
{
	int ret = -1;
	int i = 0;
	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_t * p = mem_arr_get( listens, i + 1 );
		if( OK != listen_open( p ) ) {
			err("listen open failed\n");
			break;
		}
	}
	if( i >= listens->elem_num ) ret = 0;

	if( ret == 0 ) {
		return OK;
	} else {
		listen_stop();
		return ERROR;
	}
}


int listen_num( )
{
	return ( listens ? listens->elem_num : 0 );
}

status listen_init( void )
{	
    if( OK != mem_arr_create( &listens, sizeof(listen_t) ) )  {
        err("listens arr create failed\n" );
        return ERROR;
    }

    // s5 local listen
    if( config_get()->s5_mode == SOCKS5_CLIENT )
        listen_add( config_get()->s5_local_port, socks5_local_accept_cb, L_NOSSL );
    // s5 server listen
    if( config_get()->s5_mode == SOCKS5_SERVER )
        listen_add( config_get()->s5_serv_port, socks5_server_accept_cb, L_SSL );
    // webserver listen
    for( int i = 0; i < config_get()->http_num; i ++ )
        listen_add( config_get()->http_arr[i], webser_accept_cb, L_NOSSL );
    // webserver ssl listen
    for( int i = 0; i < config_get()->https_num; i ++ )
        listen_add( config_get()->https_arr[i], webser_accept_cb_ssl, L_SSL );

    if( OK != listen_start() ) {
		err("listen start failed\n");
		return ERROR;
    }
    return OK;
}

status listen_end( void )
{
	if( listens ) {
		mem_arr_free( listens );
		listens = NULL;
	}
	return OK;
}

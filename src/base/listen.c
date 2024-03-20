#include "common.h"
#include "dns.h"
#include "s5_server.h"
#include "s5_local.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"

listen_t * g_listens = NULL;
static int g_listenn = 0;

static status listen_add( unsigned short port, event_pt handler, char fssl )
{
    listen_t * nl = sys_alloc( sizeof(listen_t) );
    if(!nl){
        err("listne alloc nl failed\n");
        return ERROR;
    }
    nl->lport = port;
    nl->fssl = fssl;
    nl->handler = handler;

    if( !g_listens ) {
        g_listens = nl;
    } else {
        listen_t * p = g_listens;
        while(p->next) {
            p = p->next;
        }
        p->next = nl;
    }
    g_listenn++;
	return OK;
}

static status listen_open( listen_t * lev )
{
	/// listen must be tcp
	lev->server_addr.sin_family = AF_INET;
	lev->server_addr.sin_port = htons( lev->lport );
	lev->server_addr.sin_addr.s_addr = htonl( INADDR_ANY );

	do {
		lev->fd = socket( AF_INET, SOCK_STREAM, 0 );
		if( -1 == lev->fd )  {
			err("listen open listen socket failed\n");
			break;
		}
		if( OK != net_socket_nbio( lev->fd ) )  {
			err("listen set socket non blocking failed\n");
			break;
		}
		
		if( OK != net_socket_reuseaddr( lev->fd ) )	{
			err("listen set socket reuseaddr failed\n" );
			break;
		}
		
			
		/*
			set reuseport flag, socket listen by all process.
			kernel will be process thundering herd 
		*/
		
		if( OK != net_socket_reuseport( lev->fd ) )	{
			err("listen set socket reuseport failed\n" );
			break;
		}
		if( OK != net_socket_fastopen( lev->fd ) ) {
			err("listen set socket fastopen failed\n" );
			break;
		}
		if( OK != net_socket_nodelay( lev->fd ) ){
			err("listen set socket nodelay failed\n" );
			break;
		}
		
		if( OK != bind( lev->fd, (struct sockaddr *)&lev->server_addr, sizeof(struct sockaddr) ) ) {
			err("listen bind failed, [%d]\n", errno );
			break;
		}
		if( OK != listen( lev->fd, 10 ) ) {
			err("listen call listen failed\n" );
			break;
		}
		return OK;
	}while(0);

	return ERROR;
}

status listen_start( void )
{
    int ret = 0;
    listen_t * p = g_listens;
    while( p ) {
        if(OK != listen_open(p)) {
            err("listen open failed\n");
            ret = -1;
            break;
        }
        p = p->next;
    }

    if(ret != 0) {
        p = g_listens;
        while(p) {
            if(p->fd > 0) {
                close(p->fd);
            }
        }
        return ERROR;
    }
    return OK;
}

status listen_init( void )
{	
	int i = 0;

	// s5 local listen
	if( config_get()->s5_mode == SOCKS5_CLIENT ) {
		listen_add( config_get()->s5_local_port, s5_local_accept_cb, S5_NOSSL );    /// local no ssl
	}
	// s5 server listen
	if( config_get()->s5_mode == SOCKS5_SERVER ) {
		listen_add( config_get()->s5_serv_port, s5_server_accept_cb, S5_SSL );  //// server ssl
	}
	// webserver listen
	for( i = 0; i < config_get()->http_num; i ++ ) {
		listen_add( config_get()->http_arr[i], webser_accept_cb, S5_NOSSL );
	}
	// webserver ssl listen
	for( i = 0; i < config_get()->https_num; i ++ ) {
		listen_add( config_get()->https_arr[i], webser_accept_cb_ssl, S5_SSL );
	}

	if( OK != listen_start() ) {
		err("listen start failed\n");
		return ERROR;
	}
	return OK;
}

status listen_end( void )
{
	if( g_listens ) {
        listen_t * p = g_listens;
        listen_t * n = NULL;
        while(p) {
            n = p->next;
            sys_free(p);
            p = n;
        }
    }
	return OK;
}

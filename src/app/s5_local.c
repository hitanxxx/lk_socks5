#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

static struct sockaddr_in s5_serv_addr;



static status s5_local_auth_send( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->up->meta;
    status rc = 0;

    rc = up->send_chain( up, meta );
    if( rc < 0 ) {
        if( rc == ERROR ) {
            err("s5 local send authorization data failed\n");
            s5_free( s5 );
            return ERROR;
        }
        timer_set_data( &ev->timer, s5 );
        timer_set_pt( &ev->timer, s5_timeout_cb );
        timer_add( &ev->timer, S5_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );
    /// s5 auth request send finish, goto recv the response
    /// reset the meta
    meta->pos = meta->last = meta->start;

    s5->down->event->read_pt = s5_traffic_process;
    s5->down->event->write_pt = NULL;
    return s5->down->event->read_pt( s5->down->event );
}

static status s5_local_auth_build( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    s5_auth_info_t * header = NULL;
    s5_auth_data_t * payload = NULL;

    /// fill in s5_auth_info_t
    meta->pos = meta->last = meta->start;
    header = (s5_auth_info_t*)meta->last;
    header->magic = htonl(S5_AUTH_MAGIC_NUM);
    header->typ = S5_MSG_LOGIN_REQ;
    header->code = S5_ERR_SUCCESS;
    meta->last += sizeof(s5_auth_info_t);
	
    /// fill in s5_auth_data_t
    payload = (s5_auth_data_t*)(meta->last);
    memset( payload->auth, 0, sizeof(payload->auth) );
    memcpy( (char*)payload->auth, config_get()->s5_local_auth, sizeof(payload->auth) );
    meta->last += sizeof(s5_auth_data_t);

    /// goto send s5 private authorization login request
    ev->write_pt = s5_local_auth_send;
    return ev->write_pt( ev );
}

static inline void s5_local_up_addr_get( struct sockaddr_in * addr )
{
	memcpy( addr, &s5_serv_addr, sizeof(struct sockaddr_in) );
}


static status s5_local_up_connect_ssl( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;

    if( !up->ssl->handshaked ) {
        err("s5 local connect to s5 server failed. ssl handshake error\n" );
        s5_free( cycle );
        return ERROR;
    }
    timer_del( &ev->timer );
    
    up->recv = ssl_read;
    up->send = ssl_write;
    up->recv_chain = NULL;
    up->send_chain = ssl_write_chain;

    ev->write_pt = s5_local_auth_build;
    return ev->write_pt( ev );
}

static status s5_local_up_connect_check( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    status rc;

    do {
        if( OK != net_socket_check_status( up->fd ) ) {
            err("s5 local connect check status failed\n");
            break;
        }
        timer_del( &ev->timer );
        net_socket_nodelay( up->fd );
        
        /// must use ssl connect s5 server !!!
        up->ssl_flag = 1;        
        if( up->ssl_flag ) {
            if( OK != ssl_create_connection( up, L_SSL_CLIENT ) ) {
                err("s5 local create ssl connection for up failed\n");
                break;
            }
            rc = ssl_handshake( up->ssl );
            if( rc < 0 ) {
                if( rc != AGAIN ) {
                    err("s5 local ssl handshake failed\n");
                    break;
                }
                up->ssl->cb = s5_local_up_connect_ssl;
                timer_set_data( &ev->timer, cycle );
                timer_set_pt( &ev->timer, s5_timeout_cb );
                timer_add( &ev->timer, S5_TIMEOUT );
                return AGAIN;
            }
            return s5_local_up_connect_ssl( ev );
        }
        ev->write_pt = s5_local_auth_build;
        return ev->write_pt( ev );
    } while(0);

    s5_free( cycle );
    return ERROR;
}

static status s5_local_down_recv( event_t * ev )
{
    /// cache read data
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    meta_t * meta = down->meta;
    int readn = 0;

    while( 1 ) {

        /// check meta remain space
        if( meta->end <= meta->last ) {
            err("s5 local down recv cache data too much\n");
            s5_free(s5);
            return ERROR;
        }

        /// cache read data
        readn = down->recv( down, meta->last, meta->end - meta->last );
        if( readn < 0 ) {
            if( readn == ERROR ) {
                err("s5 local down recv failed\n");
                s5_free(s5);
                return ERROR;
            }
            timer_set_data( &s5->up->event->timer, s5 );
            timer_set_pt( &s5->up->event->timer, s5_timeout_cb );
            timer_add( &s5->up->event->timer, S5_TIMEOUT );
            return AGAIN;
        }
        meta->last += readn;
    }
}

status s5_local_accept_cb( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;
    status rc;

    down->event->read_pt = s5_local_down_recv;
    down->event->write_pt = NULL;
    event_opt( down->event, down->fd, EV_R );

    do {
        /// alloc down mem and meta
        if( !down->page ) {
            if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) ) {
                err("s5 down page alloc failed\n");
                break;
            }
        }

        if( !down->meta ) {
            if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) ) {
                err("s5 down meta alloc failed\n");
                break;
            }
        }
    
        /// alloc up and goto connect
        if( OK != s5_alloc( &s5 ) ) {
            err("s5 cycle alloc failed\n");
            net_free( down );
            return ERROR;
        }
        s5->typ = SOCKS5_CLIENT;
        if( OK != net_alloc( &s5->up ) ) {
            err("s5 up alloc failed\n");
            break;
        }
        s5->up->data = s5;
        s5->down = down;
        s5->down->data = s5;
        
        if( !s5->up->page ) {
            if( OK != mem_page_create(&s5->up->page, L_PAGE_DEFAULT_SIZE) ) {
                err("s5 up page alloc failed\n");
                break;
            }
        }

        if( !s5->up->meta ) {
            if( OK != meta_alloc_form_mempage( s5->up->page, 4096, &s5->up->meta ) ) {
                err("s5 up meta alloc failed\n");
                break;
            }
        }

        s5_local_up_addr_get( &s5->up->addr );
        rc = net_connect( s5->up, &s5->up->addr );
        if( rc == ERROR ) {
            err("cycle up connect failed\n");
            break;
        }
        s5->up->event->read_pt	= NULL;
        s5->up->event->write_pt	= s5_local_up_connect_check;
        if( rc == AGAIN ) {
            if( s5->up->event->opt != EV_W ) {
                event_opt( s5->up->event, s5->up->fd, EV_W );
            }
            timer_set_data( &s5->up->event->timer, s5 );
            timer_set_pt( &s5->up->event->timer, s5_timeout_cb );
            timer_add( &s5->up->event->timer, S5_TIMEOUT );
            return AGAIN;
        }
        return s5->up->event->write_pt( s5->up->event );
    } while(0);

    s5_free( s5 );
    return ERROR;
}

status socks5_local_init( void )
{
	// init s5 server add for use
	memset( &s5_serv_addr, 0, sizeof(struct sockaddr_in) );
	s5_serv_addr.sin_family = AF_INET;
    s5_serv_addr.sin_port = htons( config_get()->s5_local_serv_port );
    s5_serv_addr.sin_addr.s_addr = inet_addr( config_get()->s5_local_serv_ip );
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}



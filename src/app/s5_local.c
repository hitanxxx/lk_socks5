#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

static struct sockaddr_in s5_serv_addr;


static status s5_local_auth_recv( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;
    ssize_t rc = 0;

    while( meta_len( meta->pos, meta->last ) < sizeof(s5_auth_info_t) ) {
        rc = up->recv( up, meta->last, meta_len( meta->last, meta->end ) );
        if( rc < 0 ) {
            if( rc == ERROR ) {
                err("s5 local authorization recv failed\n");
                s5_free( s5 );
                return ERROR;
            }
            timer_set_data( &ev->timer, s5 );
            timer_set_pt(&ev->timer, s5_timeout_cb );
            timer_add( &ev->timer, S5_TIMEOUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );

	do {
        s5_auth_info_t * header = (s5_auth_info_t*) meta->pos;

        if( S5_AUTH_MAGIC_NUM != header->magic ) {
            err("s5 auth, magic [0x%x] incorrect, should be [0x%x]\n", header->magic, S5_AUTH_MAGIC_NUM );
            break;
        }

        if( S5_MSG_LOGIN_RESP != header->typ ) {
            err("s5 auth, msg type [0x%x] incorrect, not S5_MSG_LOGIN_RESP [0x%x]\n", header->typ, S5_MSG_LOGIN_RESP );
            break;
        }

        if( S5_ERR_SUCCESS != header->code ) {
            err("s5 auth, msg errcode [0x%x] incorrect. shoud be success [0x%x] %x\n", header->code, S5_ERR_SUCCESS );
            break;
        }

        ev->read_pt	= s5_traffic_process;
        ev->write_pt = NULL;
        return ev->read_pt( ev );
    } while(0);

   	s5_free( s5 );
    return ERROR;
}

static status s5_local_auth_send( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;
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

    ev->write_pt = NULL;
    ev->read_pt = s5_local_auth_recv;
    event_opt( ev, up->fd, EV_R );
    return ev->read_pt( ev );
}

static status s5_local_auth_build( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;

    s5_auth_info_t * header = NULL;
    s5_auth_data_t * payload = NULL;

    /// fill in s5_auth_info_t
    meta->pos = meta->last = meta->start;
    header = (s5_auth_info_t*)meta->last;
    header->magic = S5_AUTH_MAGIC_NUM;
    header->typ = S5_MSG_LOGIN_REQ;
    header->code = S5_ERR_SUCCESS;
    meta->last += sizeof(s5_auth_info_t);
	
    /// fill in s5_auth_data_t
    payload = (s5_auth_data_t*)(meta->last);
    memcpy( (char*)payload->auth, config_get()->s5_local_auth, sizeof(payload->auth) );
    meta->last += sizeof(s5_auth_data_t);

    /// goto send s5 private authorization login request
    ev->write_pt = s5_local_auth_send;
    return ev->write_pt( ev );
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
        cycle->up->ssl_flag  = 1;

        
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
        ev->write_pt	= s5_local_auth_build;
        return ev->write_pt( ev );
    } while(0);

    s5_free( cycle );
    return ERROR;
}

static inline void s5_local_up_addr_get( struct sockaddr_in * addr )
{
	memcpy( addr, &s5_serv_addr, sizeof(struct sockaddr_in) );
}

status s5_local_accept_cb( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;
    status rc;

    down->event->read_pt = NULL;
    down->event->write_pt = NULL;
    // make down event invalidate, wait up connect server success
    event_opt( down->event, down->fd, EV_NONE );

    do {
        if( OK != s5_alloc( &s5 ) ) {
            err("s5 local alloc cycle failed\n");
            net_free( down );
            return ERROR;
        }
        down->data 	= s5;
        s5->down = down;

        if( !down->page ) {
            if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) ) {
                err("webser c page create failed\n");
                break;
            }
        }

        if( !down->meta ) {
            if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) ) {
                err("s5 alloc down meta failed\n");
                break;
            }
        }

        if( OK != net_alloc( &s5->up ) ) {
            err("cycle up alloc failed\n");
            break;
        }
        s5->up->data = s5;

        s5_local_up_addr_get( &s5->up->addr );
        rc = net_connect( s5->up, &s5->up->addr );
        if( rc == ERROR ) {
            err("cycle up connect failed\n");
            break;
        }
        s5->up->event->read_pt	= NULL;
        s5->up->event->write_pt	= s5_local_up_connect_check;
        event_opt( s5->up->event, s5->up->fd, EV_W );
        if( rc == AGAIN ) {
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
    s5_serv_addr.sin_port = htons( (uint16_t)config_get()->s5_local_serv_port );
    s5_serv_addr.sin_addr.s_addr = inet_addr( (char*)config_get()->s5_local_serv_ip );
	
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}



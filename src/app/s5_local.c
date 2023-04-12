#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

static struct sockaddr_in  s5_serv_addr;


static status socks5_local_msg_pri_recv( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;
    ssize_t rc = 0;

    while( meta_len( meta->pos, meta->last ) < sizeof(socks5_auth_header_t) )
    {
        rc = up->recv( up, meta->last, meta_len( meta->last, meta->end ) );
        if( rc < 0 )
        {
            if( rc == ERROR )
            {
                err("s5 local authorization recv failed\n");
                s5_free( s5 );
                return ERROR;
            }
            timer_set_data( &ev->timer, s5 );
            timer_set_pt(&ev->timer, s5_timeout );
            timer_add( &ev->timer, SOCKS5_TIME_OUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );

	socks5_auth_header_t * head = (socks5_auth_header_t*) meta->pos;
	do
    {
        head = (socks5_auth_header_t*) meta->pos;
        if( S5_AUTH_MAGIC_NUM != head->magic )
        {
            err("s5 priv, magic [%x] incorrect, should be %x\n", head->magic, S5_AUTH_MAGIC_NUM );
            break;
        }

        if( S5_AUTH_TYPE_AUTH_RESP != head->message_type )
        {
            err("s5 priv, msg type type [%x] incorrect, should be %x\n", head->message_type, S5_AUTH_TYPE_AUTH_RESP );
            break;
        }

        if( S5_AUTH_STAT_SUCCESS != head->message_status )
        {
            err("s5 priv, status [%x] incorrect. shoud be sucess %x\n", head->message_status, S5_AUTH_STAT_SUCCESS );
            break;
        }

        ev->read_pt		= socks5_traffic_transfer;
        ev->write_pt	= NULL;
        return ev->read_pt( ev );
    } while(0);

   	s5_free( s5 );
    return ERROR;
}

static status socks5_local_msg_pri_send( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;
    status rc = 0;

    rc = up->send_chain( up, meta );
    if( rc < 0 )
    {
        if( rc == ERROR )
        {
            err("s5 local send authorization data failed\n");
            s5_free( s5 );
            return ERROR;
        }
        timer_set_data( &ev->timer, s5 );
        timer_set_pt( &ev->timer, s5_timeout );
        timer_add( &ev->timer, SOCKS5_TIME_OUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

    meta->pos = meta->last = meta->start;
    ev->write_pt	= NULL;
    ev->read_pt		= socks5_local_msg_pri_recv;
    event_opt( ev, up->fd, EV_R );
    return ev->read_pt( ev );
}

static status socks5_local_msg_pri_send_build( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->down->meta;
    socks5_auth_header_t * head     = NULL;
    
    head = (socks5_auth_header_t*)meta->last;
    head->magic             = S5_AUTH_MAGIC_NUM;
    head->message_type      = S5_AUTH_TYPE_AUTH_REQ;
    head->message_status    = 0;
	
	memcpy( head->data.name, config_get()->s5_local_usrname, USERNAME_LENGTH );
	memcpy( head->data.passwd, config_get()->s5_local_passwd, PASSWD_LENGTH );
	
    meta->last += sizeof(socks5_auth_header_t);

    ev->write_pt = socks5_local_msg_pri_send;
    return ev->write_pt( ev );
}

static status socks5_local_up_connect_ssl( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;

    if( !up->ssl->handshaked ) 
    {
        err("s5 local connect to s5 server failed. ssl handshake error\n" );
        s5_free( cycle );
        return ERROR;
    }
    timer_del( &ev->timer );
    debug("s5 local fd [%d] connect to s5 server ssl success!!!\n", ev->fd );

    up->recv 		= ssl_read;
    up->send 		= ssl_write;
    up->recv_chain 	= NULL;
    up->send_chain 	= ssl_write_chain;

    ev->write_pt = socks5_local_msg_pri_send_build;
    return ev->write_pt( ev );
}

static status socks5_local_up_connect_check( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    status rc;

    do
    {
        if( OK != net_socket_check_status( up->fd ) )
        {
            err("s5 local connect check status failed\n");
            break;
        }
        timer_del( &ev->timer );
        net_socket_nodelay( up->fd );
        
        // s5 local use ssl transport with s5 server
        cycle->up->ssl_flag  = 1;
        if( up->ssl_flag )
        {
            if( OK != ssl_create_connection( up, L_SSL_CLIENT ) )
            {
                err("s5 local create ssl connection for up failed\n");
                break;
            }
            rc = ssl_handshake( up->ssl );
            if( rc < 0 )
            {
                if( rc != AGAIN )
                {
                    err("s5 local ssl handshake failed\n");
                    break;
                }
                up->ssl->cb 	= socks5_local_up_connect_ssl;
                timer_set_data( &ev->timer, cycle );
                timer_set_pt( &ev->timer, s5_timeout );
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
                return AGAIN;
            }
            return socks5_local_up_connect_ssl( ev );
        }
        ev->write_pt	= socks5_local_msg_pri_send_build;
        return ev->write_pt( ev );
    } while(0);

    s5_free( cycle );
    return ERROR;
}

static inline void socks5_local_up_addr_get( struct sockaddr_in * addr )
{
	memcpy( addr, &s5_serv_addr, sizeof(struct sockaddr_in) );
}

status socks5_local_accept_cb( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;
    status rc;

    down->event->read_pt 	= NULL;
    down->event->write_pt 	= NULL;
    // make down event invalidate, wait up connect server success
    event_opt( down->event, down->fd, EV_NONE );

    do
    {
        if( OK != s5_alloc( &s5 ) )
        {
            err("s5 local alloc cycle failed\n");
            net_free( down );
            return ERROR;
        }
        down->data 	= s5;
        s5->down = down;

        if( !down->page )
        {
            if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) )
            {
                err("webser c page create failed\n");
                break;
            }
        }

        if( !down->meta )
        {
            if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) )
            {
                err("s5 alloc down meta failed\n");
                break;
            }
        }

        if( OK != net_alloc( &s5->up ) )
        {
            err("cycle up alloc failed\n");
            break;
        }
        s5->up->data		= s5;

        socks5_local_up_addr_get( &s5->up->addr );
        rc = net_connect( s5->up, &s5->up->addr );
        if( rc == ERROR )
        {
            err("cycle up connect failed\n");
            break;
        }
        s5->up->event->read_pt	= NULL;
        s5->up->event->write_pt	= socks5_local_up_connect_check;
        event_opt( s5->up->event, s5->up->fd, EV_W );
        if( rc == AGAIN )
        {
            timer_set_data( &s5->up->event->timer, s5 );
            timer_set_pt( &s5->up->event->timer, s5_timeout );
            timer_add( &s5->up->event->timer, SOCKS5_TIME_OUT );
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
	s5_serv_addr.sin_family 		= AF_INET;
    s5_serv_addr.sin_port 			= htons( (uint16_t)config_get()->s5_local_serv_port );
    s5_serv_addr.sin_addr.s_addr	= inet_addr( (char*)config_get()->s5_local_serv_ip );
	
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}



#include "l_base.h"
#include "l_dns.h"
#include "l_socks5_local.h"
#include "l_socks5_server.h"

static struct sockaddr_in  s5_serv_addr;


static status socks5_local_authorization_resp_recv( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    meta_t * meta = &cycle->up2down_meta;
    ssize_t rc = 0;

    while( meta_len( meta->pos, meta->last ) < sizeof(socks5_auth_header_t) )
    {
        rc = up->recv( up, meta->last, meta_len( meta->last, meta->end ) );
        if( rc < 0 )
        {
            if( rc == ERROR )
            {
                err("s5 local authorization recv failed\n");
                socks5_cycle_over( cycle );
                return ERROR;
            }
            timer_set_data( &ev->timer, cycle );
            timer_set_pt(&ev->timer, socks5_timeout_cycle );
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
            err("s5 local auth check failed, magic [%x]\n", head->magic );
            break;
        }

        if( S5_AUTH_TYPE_AUTH_RESP != head->message_type )
        {
            err("s5 local auth check msg type failed, type [%x]\n", head->message_type );
            break;
        }

        if( S5_AUTH_STAT_SUCCESS != head->message_status )
        {
            err("s5 local auth check stat failed, stat [%x]\n", head->message_status );
            break;
        }

        ev->read_pt		= socks5_pipe;
        ev->write_pt	= NULL;
        return ev->read_pt( ev );
    } while(0);

   	socks5_cycle_over( cycle );
    return ERROR;
}

static status socks5_local_authorization_req_send( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    meta_t * meta = &cycle->up2down_meta;
    status rc = 0;

    rc = up->send_chain( up, meta );
    if( rc < 0 )
    {
        if( rc == ERROR )
        {
            err("s5 local send authorization data failed\n");
            socks5_cycle_over( cycle );
            return ERROR;
        }
        timer_set_data( &ev->timer, cycle );
        timer_set_pt( &ev->timer, socks5_timeout_cycle );
        timer_add( &ev->timer, SOCKS5_TIME_OUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

    meta->pos = meta->last = meta->start;
    ev->write_pt	= NULL;
    ev->read_pt		= socks5_local_authorization_resp_recv;
    event_opt( ev, up->fd, EV_R );
    return ev->read_pt( ev );
}

static status socks5_local_authorization_req_build( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    meta_t * meta = &cycle->up2down_meta;
    socks5_auth_header_t * head     = NULL;
    
    meta->pos = meta->last = meta->start = cycle->up2down_buffer;
    meta->end = meta->start + SOCKS5_META_LENGTH;

    head = (socks5_auth_header_t*)meta->last;
    head->magic             = S5_AUTH_MAGIC_NUM;
    head->message_type      = S5_AUTH_TYPE_AUTH_REQ;
    head->message_status    = 0;
	
	memcpy( head->data.name, conf.socks5.client.user, USERNAME_LENGTH );
	memcpy( head->data.passwd, conf.socks5.client.passwd, PASSWD_LENGTH );
	
    meta->last += sizeof(socks5_auth_header_t);

    ev->write_pt = socks5_local_authorization_req_send;
    return ev->write_pt( ev );
}

static status socks5_local_server_connect_handshake( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;

    if( !up->ssl->handshaked ) {
        err(" handshake error\n" );
        socks5_cycle_over( cycle );
        return ERROR;
    }
    timer_del( &ev->timer );

    up->recv 		= ssl_read;
    up->send 		= ssl_write;
    up->recv_chain 	= NULL;
    up->send_chain 	= ssl_write_chain;

    ev->write_pt = socks5_local_authorization_req_build;
    return ev->write_pt( ev );
}

static status socks5_local_server_connect_check( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;
    status rc;

    do
    {
        if( OK != l_socket_check_status( up->fd ) )
        {
            err("s5 local connect check status failed\n");
            break;
        }
        timer_del( &ev->timer );
        l_socket_nodelay( up->fd );
        
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
                up->ssl->cb 	= socks5_local_server_connect_handshake;
                timer_set_data( &ev->timer, cycle );
                timer_set_pt( &ev->timer, socks5_timeout_cycle );
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
                return AGAIN;
            }
            return socks5_local_server_connect_handshake( ev );
        }
        ev->write_pt	= socks5_local_authorization_req_build;
        return ev->write_pt( ev );
    } while(0);

    socks5_cycle_over( cycle );
    return ERROR;
}

static inline void socks5_local_server_addr_get( struct sockaddr_in * addr )
{
	memcpy( addr, &s5_serv_addr, sizeof(struct sockaddr_in) );
}

static status socks5_local_cycle_init( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * cycle = NULL;
    status rc;

    down->event->read_pt 	= NULL;
    down->event->write_pt 	= NULL;
    // make down event invalidate, wait up connect server success
    event_opt( down->event, down->fd, EV_NONE );

    do
    {
        if( OK != socks5_cycle_alloc( &cycle ) )
        {
            err("s5 local alloc cycle failed\n");
            net_free( down );
            return ERROR;
        }
        down->data 	= cycle;
        cycle->down = down;

        if( OK != net_alloc( &cycle->up ) )
        {
            err("cycle up alloc failed\n");
            break;
        }
        cycle->up->data		= cycle;
        
        socks5_local_server_addr_get( &cycle->up->addr );
        rc = l_net_connect( cycle->up, &cycle->up->addr, TYPE_TCP );
        if( rc == ERROR )
        {
            err("cycle up connect failed\n");
            break;
        }
        cycle->up->event->read_pt	= NULL;
        cycle->up->event->write_pt	= socks5_local_server_connect_check;
        event_opt( cycle->up->event, cycle->up->fd, EV_W );
        if( rc == AGAIN )
        {
            timer_set_data( &cycle->up->event->timer, cycle );
            timer_set_pt( &cycle->up->event->timer, socks5_timeout_cycle );
            timer_add( &cycle->up->event->timer, SOCKS5_TIME_OUT );
            return AGAIN;
        }
        return cycle->up->event->write_pt( cycle->up->event );
    } while(0);

    socks5_cycle_over( cycle );
    return ERROR;
}

status socks5_local_init( void )
{
	// init s5 server add for use
	memset( &s5_serv_addr, 0, sizeof(struct sockaddr_in) );
	s5_serv_addr.sin_family 		= AF_INET;
    s5_serv_addr.sin_port 			= htons( (uint16_t)conf.socks5.client.server_port );
    s5_serv_addr.sin_addr.s_addr	= inet_addr( (char*)conf.socks5.client.server_ip );

    if( conf.socks5_mode == SOCKS5_CLIENT )
    {
        // socks5 client connect s5 local use normal tcp connection
        listen_add( conf.socks5.client.local_port, socks5_local_cycle_init, L_NOSSL );
    }
	return OK;
}

status socks5_local_end( void )
{
	return OK;
}


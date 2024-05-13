#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"


static status s5_local_auth_send( event_t * ev )
{
    con_t* up = ev->data;
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
    con_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    s5_auth_t * auth = NULL;

    /// fill in s5_auth_t
    meta->pos = meta->last = meta->start;
    auth = (s5_auth_t*)meta->last;
    auth->magic = htonl(S5_AUTH_LOCAL_MAGIC);
    memset( auth->key, 0, sizeof(auth->key) );
    memcpy( (char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key) );
    meta->last += sizeof(s5_auth_t);

#ifndef S5_OVER_TLS
    if( !s5->cipher_enc ) {
        if( 0 != sys_cipher_ctx_init( &s5->cipher_enc, 0 ) ) {
            err("s5 local cipher enc init failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( sizeof(s5_auth_t) != sys_cipher_conv( s5->cipher_enc, meta->pos, sizeof(s5_auth_t) ) ) {
        err("s5 local cipher enc meta failed\n");
        s5_free(s5);
        return ERROR;
    }
#endif
    /// goto send s5 private authorization login request
    ev->write_pt = s5_local_auth_send;
    return ev->write_pt( ev );
}

static inline void s5_local_up_addr_get( struct sockaddr_in * addr )
{
    memset( addr, 0, sizeof(struct sockaddr_in) );
    addr->sin_family = AF_INET;
    addr->sin_port = htons( config_get()->s5_local_serv_port );
    addr->sin_addr.s_addr = inet_addr( config_get()->s5_local_serv_ip );
}

static status s5_local_up_connect_ssl( event_t * ev )
{
    con_t* up = ev->data;
    socks5_cycle_t * cycle = up->data;

    if( !up->ssl->f_handshaked ) {
        err("s5 local to s5 server. ssl handshake err\n" );
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
    con_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;
    status rc;

    timer_del( &ev->timer );
    net_socket_nodelay( up->fd );
    
    if( OK != net_socket_check_status( up->fd ) ) {
        err("s5 local connect check status failed\n");
        s5_free(s5);
        return ERROR;
    }

    up->fssl = 1;
#ifndef S5_OVER_TLS
    up->fssl = 0;
#endif    
    if( up->fssl ) {
        if( OK != ssl_create_connection( up, L_SSL_CLIENT ) ) {
            err("s5 local create ssl connection for up failed\n");
            s5_free(s5);
            return ERROR;
        }
        rc = ssl_handshake( up->ssl );
        if(rc<0) {
            if(rc==AGAIN) {
                up->ssl->cb = s5_local_up_connect_ssl;
                timer_set_data( &ev->timer, s5 );
                timer_set_pt( &ev->timer, s5_timeout_cb );
                timer_add( &ev->timer, S5_TIMEOUT );
                return AGAIN;
            }
            err("s5 local ssl handshake failed\n");
            s5_free(s5);
            return ERROR;
        }
        return s5_local_up_connect_ssl( ev );
    }
    ev->write_pt = s5_local_auth_build;
    return ev->write_pt( ev );
}

static status s5_local_down_recv( event_t * ev )
{
    /// cache read data
    con_t * down = ev->data;
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
        if(readn<0) {
            if(readn==AGAIN) {
                timer_set_data( &s5->up->event->timer, s5 );
                timer_set_pt( &s5->up->event->timer, s5_timeout_cb );
                timer_add( &s5->up->event->timer, S5_TIMEOUT );
                return AGAIN;
            }
            err("s5 local down recv failed\n");
            s5_free(s5);
            return ERROR;
        }
        meta->last += readn;
    }
}

status s5_local_accept_cb( event_t * ev )
{
    con_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;

    down->event->read_pt = s5_local_down_recv;
    down->event->write_pt = NULL;
    event_opt( down->event, down->fd, EV_R );

    /// alloc down mem and meta
    if(!down->meta) {
        if(OK != meta_alloc( &down->meta, 8192 )) {
            err("s5 local alloc down meta failed\n");
            net_free(down);
            return ERROR;
        }
    }
    
    if( OK != s5_alloc(&s5) ) {
        err("s5 cycle alloc failed\n");
        net_free( down );
        return ERROR;
    }
    s5->typ = SOCKS5_CLIENT;
    s5->down = down;
    down->data = s5;

    if( OK != net_alloc( &s5->up ) ) {
        err("s5 up alloc failed\n");
        s5_free(s5);
        return ERROR;
    }
    s5->up->data = s5;
    if( !s5->up->meta ) {
        if( OK != meta_alloc( &s5->up->meta, 8192 ) ) {
            err("s5 up meta alloc failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    s5_local_up_addr_get( &s5->up->addr );

    s5->up->event->read_pt	= NULL;
    s5->up->event->write_pt	= s5_local_up_connect_check;
        
    int rc = net_connect( s5->up, &s5->up->addr );
    if(rc<0) {
        if(rc == AGAIN) {
            if( s5->up->event->opt != EV_W ) {
                event_opt( s5->up->event, s5->up->fd, EV_W );
            }
            timer_set_data( &s5->up->event->timer, s5 );
            timer_set_pt( &s5->up->event->timer, s5_timeout_cb );
            timer_add( &s5->up->event->timer, S5_TIMEOUT );
            return AGAIN;
        }
        err("cycle up connect failed\n");
        s5_free(s5);
        return ERROR;
    }
    return s5->up->event->write_pt( s5->up->event );
}

status socks5_local_init( void )
{
    return OK;
}

status socks5_local_end( void )
{
    return OK;
}



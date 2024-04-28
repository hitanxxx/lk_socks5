#include "common.h"


status net_socket_nbio( int32 fd )
{
    int flags = fcntl( fd, F_GETFL, 0 );
    if( -1 == flags ) {
        err("fcntl get failed. [%d]\n", errno );
        return ERROR;
    }
    int rc = fcntl( fd, F_SETFL, flags | O_NONBLOCK );
    if( -1 == rc ) {
        err("fcntl set nonblk failed. [%d]\n", errno );
        return ERROR;
    } 
    return OK;
}

status net_socket_reuseport( int32 fd )
{
    int tcp_reuseport = 1;
    return setsockopt( fd, SOL_SOCKET, SO_REUSEPORT, (const void *)&tcp_reuseport, sizeof(int));    
}

status net_socket_reuseaddr( int32 fd )
{    
    int tcp_reuseaddr = 1;
    return setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&tcp_reuseaddr, sizeof(int));
}

status net_socket_fastopen( int32 fd )
{
    if(0) {
        int  tcp_fastopen = 1;
        return setsockopt( fd, IPPROTO_TCP, TCP_FASTOPEN, (const void *) &tcp_fastopen, sizeof(tcp_fastopen));
    }
    return OK;
}

status net_socket_nodelay(  int32 fd )
{
    int tcp_nodelay = 1;
    return setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &tcp_nodelay, sizeof(int));
}

status net_socket_nopush( int32 fd )
{
#if(0)
    /// will be compile error in macintosh
    int tcp_cork = 1;
    return setsockopt( fd, IPPROTO_TCP, TCP_CORK, (const void *) &tcp_cork, sizeof(int));
#endif
    return OK;
}

status net_socket_lowat_send( int32 fd )
{
    if(0) {
        int lowat = 0;
        return setsockopt( fd, SOL_SOCKET, SO_SNDLOWAT, (const void*)&lowat, sizeof(int) );
    }
    return OK;
}

status net_socket_check_status( int32 fd )
{
    int    err = 0;
    socklen_t  len = sizeof(int);

    if (getsockopt( fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == -1 ) {
        err = errno;
    }
    if (err) {
        err("socket get a error, %d\n", errno );
        return ERROR;
    }
    return OK;
}

status net_check_ssl_valid( con_t * c )
{
    ssize_t n = 0;
    unsigned char buf = 0;
    
    n = recv( c->fd, (char*)&buf, 1, MSG_PEEK );
    if( n <= 0 ) {
        if( (n < 0) && ( (errno == AGAIN) || (errno == EWOULDBLOCK) ) ) {
            return AGAIN;
        }    
        err("net check ssl recv failed, [%d: %s]\n", errno, strerror(errno) );
        return ERROR;
    }
    /* 0x80:SSLv2  0x16:SSLv3/TLSv1 */
    if( !(buf&0x80) && (buf != 0x16) ) {
        return ERROR;
    }
    return OK;
}

status net_connect( con_t * c, struct sockaddr_in * addr )
{
    status rc = 0;
    int ret = ERROR;
    
    c->fd = socket( AF_INET, SOCK_STREAM , 0 );
    do {
        if( -1 == c->fd ) {
            err("net connect open socket failed, [%d]\n", errno );
            break;
        }
        if( OK != net_socket_reuseaddr( c->fd ) ) {
            err("net connect net socket reuseaddr failed\n" );
            break;
        }
        if( OK != net_socket_nbio( c->fd ) ) {
            err("net connect set socket nonblock failed\n" );
            break;
        }
        if( OK != net_socket_fastopen( c->fd ) ) {
            err("net connect fastopen socket failed\n");
        }
        while(1) {
            rc = connect( c->fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in) );
            if( 0 == rc ) {
                ret = OK;
            } else {
                if ( errno == EINTR ) {
                    continue;
                } else if ( (errno == EAGAIN) || (errno == EALREADY) || (errno == EINPROGRESS) ) {
                    ret = AGAIN;
                } else {
                    err("net connect failed, [%d]\n", errno );
                }
            }
            break;
        }
    } while(0);

    if( ret == ERROR ) {
        if( c->fd ) {
            close(c->fd);
            c->fd = 0;
        }
        return ERROR;
    }

    c->con_type = TYPE_TCP;
    c->send = sends;
    c->recv = recvs;
    c->send_chain = send_chains;
    c->recv_chain = NULL;

    return ret;    
}

status net_accept( event_t * ev )
{

    listen_t * listen = ev->data;
    int32 c_fd;
    con_t * c;
    socklen_t len = sizeof( struct sockaddr_in );
    
    while( 1 )  {
        
        struct sockaddr_in c_addr;
        memset( &c_addr, 0, len );
        c_fd = accept( listen->fd, (struct sockaddr *)&c_addr, &len );

        if( -1 == c_fd ) {
            if( errno == EWOULDBLOCK ||
                errno == EAGAIN ||
                errno == EINTR ||
                errno == EPROTO ||
                errno == ECONNABORTED
            ) {
                return AGAIN;
            }
            err("accept failed, [%d]\n", errno );
            return ERROR;
        }
        
        if( ERROR == net_alloc( &c ) )  {
            err("net alloc faield\n");
            close( c_fd );
            return ERROR;
        }
        memcpy( &c->addr, &c_addr, len );
        c->fd = c_fd;
        if( OK != net_socket_nbio( c->fd ) )  {
            err("socket set nonblock failed\n" );
            net_free( c );
            return ERROR;
        }
        if( OK != net_socket_nodelay( c->fd ) ) {
            err("socket set nodelay failed\n");
            net_free(c);
            return ERROR;
        }
        if( OK != net_socket_fastopen( c->fd ) ) {
            err("socket set fastopen failed\n");
        }
        if( OK != net_socket_lowat_send( c->fd ) ) {
            err("socket set lowat 0 failed\n");
            return ERROR;
        }
        
        c->con_type = TYPE_TCP;
        c->recv = recvs;
        c->send = sends;
        c->recv_chain = NULL;
        c->send_chain = send_chains;
        c->fssl = (listen->fssl) ? 1 : 0;

        c->event->read_pt = listen->handler;
        c->event->write_pt = NULL;
        
        event_opt( c->event, c->fd, EV_R );
        event_post_event( c->event );
    }
    return OK;
}


static status net_free_cb( event_t * ev )
{
    con_t * c = ev->data;
    
    if( c->event ) {
        event_free( ev );
        c->event = NULL;
    }
    
    meta_t * m = c->meta;
    meta_t * n = NULL;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    
    if( c->fd ) {
        close( c->fd );
        c->fd = 0;
    }
    c->con_type        = 0;
    c->data         = NULL;
    memset( &c->addr, 0, sizeof(struct sockaddr_in) );

    c->ssl             = NULL;
    c->fssl     = 0;
    
    c->send            = NULL;
    c->send_chain    = NULL;
    c->recv            = NULL;
    c->recv_chain    = NULL;

    mem_pool_free(c);
    return OK;
}

void net_free_ssl_timeout( void * data )
{
    con_t * c = data;

    if( AGAIN == ssl_shutdown( c->ssl ) ) {
        c->ssl->cb = net_free_cb;
        timer_set_data( &c->event->timer, c );
        timer_set_pt( &c->event->timer, net_free_ssl_timeout );
        timer_add( &c->event->timer, L_NET_TIMEOUT );
        return;
    }
    c->event->write_pt  = NULL;
    c->event->read_pt   = net_free_cb;
    c->event->read_pt( c->event );
}

status net_free( con_t * c )
{
    sys_assert(c!=NULL);

    if( c->ssl && c->fssl ) {
        if( AGAIN == ssl_shutdown( c->ssl ) ) {
            c->ssl->cb = net_free_cb;
            timer_set_data( &c->event->timer, c );
            timer_set_pt( &c->event->timer, net_free_ssl_timeout );
            timer_add( &c->event->timer, L_NET_TIMEOUT );
            return AGAIN;
        }
    }
    
    c->event->write_pt = NULL;
    c->event->read_pt = net_free_cb;
    return c->event->read_pt( c->event );
}

status net_alloc( con_t ** c )
{
    con_t * nc = mem_pool_alloc( sizeof(con_t) );
    if(!nc) {
        err("net alloc nc failed\n");
        return ERROR;
    }

    if(!nc->event) {
        if(OK != event_alloc(&nc->event)) {
            err("net alloc nc evt failed\n");
            mem_pool_free(nc);
            return ERROR;
        }
        nc->event->data = nc;
    }
    *c = nc;
    return OK;
}

void net_timeout( void * data )
{
    net_free( (con_t *)data );
}


status net_init( void )
{
    return OK;
}

status net_end( void )
{
    return OK;
}

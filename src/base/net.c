#include "common.h"


status net_socket_nbio( int32 fd )
{
    int flags = fcntl( fd, F_GETFL, 0 );
    if( -1 == flags ) {
        err("fcntl get failed. [%d]\n", errno );
        return ERROR;
    }
    if( -1 == fcntl( fd, F_SETFL, flags | O_NONBLOCK ) ) {
        err("fcntl set non-blocking failed. [%d]\n", errno );
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
    int err = 0;
    socklen_t errn = sizeof(int);
    if (getsockopt( fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errn) == -1) {
        err("socket get a error, %d\n", errno );
        return ERROR;
    }
    return OK;
}

status net_check_ssl_valid( con_t * c )
{
    unsigned char buf = 0;    
    int n = recv(c->fd, (char*)&buf, 1, MSG_PEEK );
    if(n<=0) {
        if((n < 0) && ((errno == AGAIN)||(errno == EWOULDBLOCK)) ) {
            return AGAIN;
        }    
        err("peek recv failed, <%d: %s>\n", errno, strerror(errno) );
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
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd==-1) {
        err("socket open failed. [%d]\n", errno );
        return ERROR;
    }
    if(OK!=net_socket_nbio(fd)) {
        err("socket non-blocking failed. [%d]\n", errno );
        close(fd);
        return ERROR;
    }
    if(OK!=net_socket_reuseaddr(fd)) {
        err("socket reuseaddr failed. [%d]\n", errno );
        close(fd);
        return ERROR;
    }
    if(OK!=net_socket_fastopen(fd)) {
        err("socket fastopen failed. [%d]\n", errno );
        close(fd);
        return ERROR;
    }
    for(;;) {
        int rc = connect(fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in));
        if(rc!=0) {
            if(errno==EINTR) { ///irq by signal
                continue;
            } else if((errno == EAGAIN) || (errno == EALREADY) || (errno == EINPROGRESS)) {
                c->fd = fd;
                c->send = sends;
                c->recv = recvs;
                c->send_chain = send_chains;
                return AGAIN;
            }
            err("connect failed, [%d]\n", errno );
            close(fd);
            return ERROR;
        }
        c->fd = fd;
        c->send = sends;
        c->recv = recvs;
        c->send_chain = send_chains;
        return OK;
    }
}

status net_accept( event_t * ev )
{
    listen_t * listen = ev->data;
    int32 cfd;
    con_t * ccon;
    struct sockaddr_in caddr;
    socklen_t caddrn = sizeof(struct sockaddr_in);
    
    for(;;) {
        memset( &caddr,0x0,caddrn);
        cfd = accept( listen->fd, (struct sockaddr *)&caddr, &caddrn );
        if(-1 == cfd) {
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
        if( ERROR == net_alloc( &ccon ) )  {
            err("net alloc faield\n");
            close(cfd);
            return ERROR;
        }
        memcpy(&ccon->addr,&caddr,caddrn);
        ccon->fd=cfd;
        if( OK != net_socket_nbio( ccon->fd ) )  {
            err("socket set nonblock failed\n");
            net_free(ccon);
            return ERROR;
        }
        if( OK != net_socket_nodelay( ccon->fd ) ) {
            err("socket set nodelay failed\n");
            net_free(ccon);
            return ERROR;
        }
        if( OK != net_socket_fastopen( ccon->fd ) ) {
            err("socket set fastopen failed\n");
        }
        if( OK != net_socket_lowat_send( ccon->fd ) ) {
            err("socket set lowat 0 failed\n");
            return ERROR;
        }
        ccon->recv = recvs;
        ccon->send = sends;
        ccon->send_chain = send_chains;
        ccon->fssl = (listen->fssl)?1:0;

        ccon->event->read_pt = listen->handler;
        ccon->event->write_pt = NULL;
        
        event_opt( ccon->event, cfd, EV_R );
        event_post_event( ccon->event );
    }
    return OK;
}


static status net_free_final( event_t * ev )
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
    c->data = NULL;
    memset( &c->addr, 0, sizeof(struct sockaddr_in) );

    if(c->ssl && c->fssl) {
        SSL_free( c->ssl->con );
        mem_pool_free( c->ssl );
        c->ssl = NULL;
        c->fssl = 0;
    }
    
    c->send = NULL;
    c->send_chain = NULL;
    c->recv = NULL;

    mem_pool_free(c);
    return OK;
}

void net_free_ssl_timeout( void * data )
{
    con_t * c = data;
    if(c->ssl) ssl_shutdown(c->ssl);
    net_free_final(c->event);
}

status net_free( con_t * c )
{
    sys_assert(c!=NULL);
    if( c->ssl && c->fssl ) {
        int rc = ssl_shutdown(c->ssl);
        if(rc == AGAIN) {
            c->ssl->cb = net_free_final;
            timer_set_data( &c->event->timer, c );
            timer_set_pt( &c->event->timer, net_free_ssl_timeout );
            timer_add( &c->event->timer, L_NET_TIMEOUT );
            return AGAIN;
        }
    }
    return net_free_final(c->event);   ///ERROR or OK will do
}

status net_alloc( con_t ** c )
{
    con_t * nc = mem_pool_alloc(sizeof(con_t));
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
    net_free((con_t *)data);
}

status net_init( void )
{
    return OK;
}

status net_end( void )
{
    return OK;
}

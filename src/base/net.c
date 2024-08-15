#include "common.h"

int net_socket_nbio(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    schk(flags != -1, return -1);
    schk(fcntl(fd, F_SETFL, flags|O_NONBLOCK) != -1, return -1);
    return 0;
}

int net_socket_reuseport(int fd)
{
    int tcp_reuseport = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const void *)&tcp_reuseport, sizeof(int));    
}

int net_socket_reuseaddr(int fd)
{    
    int tcp_reuseaddr = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&tcp_reuseaddr, sizeof(int));
}

int net_socket_fastopen(int fd)
{
    if(0) {
        int  tcp_fastopen = 1;
        return setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, (const void *) &tcp_fastopen, sizeof(tcp_fastopen));
    }
    return 0;
}

int net_socket_nodelay(  int fd)
{
    int tcp_nodelay = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &tcp_nodelay, sizeof(int));
}

int net_socket_nopush(int fd)
{
#if(0)
    /// will be compile error in macintosh
    int tcp_cork = 1;
    return setsockopt( fd, IPPROTO_TCP, TCP_CORK, (const void *) &tcp_cork, sizeof(int));
#endif
    return 0;
}

int net_socket_lowat_send(int fd)
{
    if(0) {
        int lowat = 0;
        return setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, (const void*)&lowat, sizeof(int));
    }
    return 0;
}

int net_socket_check_status(int fd)
{
    int err = 0;
    socklen_t errn = sizeof(int);
    schk(getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errn) != -1, return -1);
    return 0;
}

int net_check_ssl_valid(con_t * c)
{
    unsigned char buf = 0;
    int n = recv(c->fd, (char*)&buf, 1, MSG_PEEK);
    if(n < 0) {
        if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return -11;
        }
        err("peek recv err. <%d:%s>\n", errno, strerror(errno));
        return -1;
    }
    if (n == 1) {
        if(!(buf & 0x80) && (buf != 0x16)) { ///0x80:SSLv2  0x16:SSLv3/TLSv1
            return -1;
        } else {
            return 0;
        }
    }
    err("peek recv peer closed\n");
    return -1;
}

int net_connect(con_t * c, struct sockaddr_in * addr)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    schk(fd != -1, return -1);
    schk(net_socket_nbio(fd) == 0, {close(fd); return -1;});
    schk(net_socket_reuseaddr(fd) == 0, {close(fd); return -1;});
    schk(net_socket_fastopen(fd) == 0, {close(fd); return -1;});
    
    for(;;) {
        int rc = connect(fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in));
        if(rc != 0) {
            if(errno == EINTR) { ///irq by signal
                continue;
            } else if((errno == EAGAIN) || (errno == EALREADY) || (errno == EINPROGRESS)) {
                c->fd = fd;
                c->send = sends;
                c->recv = recvs;
                c->send_chain = send_chains;
                return -11;
            }
            err("connect failed, [%d]\n", errno);
            close(fd);
            return -1;
        }
        c->fd = fd;
        c->send = sends;
        c->recv = recvs;
        c->send_chain = send_chains;
        return 0;
    }
}

int net_accept(event_t * ev)
{
    listen_t * listen = ev->data;
    int cfd;
    con_t * ccon;
    struct sockaddr_in caddr;
    socklen_t caddrn = sizeof(struct sockaddr_in);
    
    for(;;) {
        memset(&caddr, 0x0, caddrn);
        cfd = accept(listen->fd, (struct sockaddr *)&caddr, &caddrn);
        if(-1 == cfd) {
            if( errno == EWOULDBLOCK ||
                errno == EAGAIN ||
                errno == EINTR ||
                errno == EPROTO ||
                errno == ECONNABORTED
            ) {
                return -11;
            }
            err("accept failed, [%d]\n", errno);
            return -1;
        }
        schk(net_alloc(&ccon) != -1, {close(cfd); return -1;});
        memcpy(&ccon->addr, &caddr, caddrn);
        ccon->fd = cfd;
        schk(net_socket_nbio(ccon->fd) == 0, {net_free(ccon); return -1;});
        schk(net_socket_nodelay(ccon->fd) == 0, {net_free(ccon); return -1;});
        schk(net_socket_fastopen(ccon->fd) == 0, {net_free(ccon); return -1;});
        schk(net_socket_lowat_send(ccon->fd) == 0, {net_free(ccon); return -1;});
        
        ccon->recv = recvs;
        ccon->send = sends;
        ccon->send_chain = send_chains;
        ccon->fssl = (listen->fssl) ? 1 : 0;

        ccon->event->read_pt = listen->handler;
        ccon->event->write_pt = NULL;
        
        event_opt(ccon->event, cfd, EV_R);
        event_post_event(ccon->event);
    }
    return 0;
}


static int net_free_final(event_t * ev)
{
    con_t * c = ev->data;
    
    if(c->event) {
        event_free(ev);
        c->event = NULL;
    }
    
    meta_t * m = c->meta;
    meta_t * n = NULL;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    
    if(c->fd) {
        close(c->fd);
        c->fd = 0;
    }
    c->data = NULL;
    memset(&c->addr, 0x0, sizeof(struct sockaddr_in));

    if(c->ssl && c->fssl) {
        SSL_free(c->ssl->con);
        mem_pool_free(c->ssl);
        c->ssl = NULL;
        c->fssl = 0;
    }
    
    c->send = NULL;
    c->send_chain = NULL;
    c->recv = NULL;
    mem_pool_free(c);
    return 0;
}

void net_free_ssl_timeout(void * data)
{
    con_t * c = data;
    if(c->ssl) ssl_shutdown(c->ssl);
    net_free_final(c->event);
}

int net_free(con_t * c)
{
    sys_assert(c != NULL);
    if(c->ssl && c->fssl) {
        int rc = ssl_shutdown(c->ssl);
        if(rc == -11) {
            c->ssl->cb = net_free_final;
            timer_set_data(&c->event->timer, c);
            timer_set_pt(&c->event->timer, net_free_ssl_timeout);
            timer_add(&c->event->timer, L_NET_TIMEOUT);
            return -11;
        }
    }
    return net_free_final(c->event); ///-1 or 0 will do
}

int net_alloc(con_t ** c)
{
    con_t * nc = mem_pool_alloc(sizeof(con_t));
    schk(nc, return -1);
    if(!nc->event) {
        schk(event_alloc(&nc->event) == 0, {mem_pool_free(nc); return -1;});
        nc->event->data = nc;
    }
    *c = nc;
    return 0;
}

void net_timeout(void * data)
{
    net_free((con_t *)data);
}

int net_init(void)
{
    return 0;
}

int net_end(void)
{
    return 0;
}

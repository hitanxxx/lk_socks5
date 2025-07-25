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
#if 0
	int  tcp_fastopen = 1;
	return setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN, (const void *) &tcp_fastopen, sizeof(tcp_fastopen));
#endif
    return 0;
}

int net_socket_nodelay(  int fd)
{
    int tcp_nodelay = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const void *) &tcp_nodelay, sizeof(int));
}

int net_socket_nopush(int fd)
{
#if 0
    /// will be compile error in macintosh
    int tcp_cork = 1;
    return setsockopt( fd, IPPROTO_TCP, TCP_CORK, (const void *) &tcp_cork, sizeof(int));
#endif
    return 0;
}

int net_socket_lowat_send(int fd)
{
#if 0
    int lowat = 0;
    return setsockopt(fd, SOL_SOCKET, SO_SNDLOWAT, (const void*)&lowat, sizeof(int));
#endif
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

    c->fd = fd;
    c->send = sends;
    c->recv = recvs;
    c->send_chain = send_chains;
    ev_opt(c, EV_R|EV_W);
    
    for(;;) {
        int rc = connect(fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in));
        if(rc != 0) {
            if(errno == EINTR) { ///irq by signal
                continue;
            } else if((errno == EAGAIN) || (errno == EALREADY) || (errno == EINPROGRESS)) {
                return -11;
            }
            err("connect failed, [%d]\n", errno);
            close(fd);
            return -1;
        }
        return 0;
    }
}

int net_accept(con_t * c)
{
    listen_t * listen = c->data;
    
    int cfd;
    con_t * cc;
    
    struct sockaddr_in caddr;
    socklen_t caddrn = sizeof(struct sockaddr_in);
    
    for(;;) {
        memset(&caddr, 0x0, caddrn);
        cfd = accept(c->fd, (struct sockaddr *)&caddr, &caddrn);
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
        schk(net_alloc(&cc) != -1, {close(cfd); return -1;});
        memcpy(&cc->addr, &caddr, caddrn);
        cc->fd = cfd;
        
        schk(net_socket_nbio(cc->fd) == 0, {net_free(cc); return -1;});
        schk(net_socket_nodelay(cc->fd) == 0, {net_free(cc); return -1;});
        schk(net_socket_fastopen(cc->fd) == 0, {net_free(cc); return -1;});
        schk(net_socket_lowat_send(cc->fd) == 0, {net_free(cc); return -1;});
        
        cc->recv = recvs;
        cc->send = sends;
        cc->send_chain = send_chains;
        cc->fssl = (listen->fssl) ? 1 : 0;

        cc->ev->read_cb = listen->cb;
        cc->ev->write_cb = NULL;
        ev_opt(cc, EV_R);

		tm_add(cc, net_exp, 5);
    }
    return 0;
}

int net_free_direct(con_t * c)
{
    if(c->fd) {
        ev_opt(c, EV_NONE);
        close(c->fd);
    }

    if(c->ev) {
        tm_del(c);
        ev_free(c->ev);
        c->ev = NULL;
    }

	if(c->meta) {
		meta_free(c->meta);
		c->meta = NULL;
	}

    if(c->data) {
        if(c->data_cb) c->data_cb(c->data);
        c->data = NULL;
    }
    
    if(c->ssl) {
        SSL_free(c->ssl->con);
        mem_pool_free(c->ssl);
    }
    mem_pool_free(c);
    return 0;
}

int net_free(con_t * c)
{
    /*
	if(is tls connection) {
		if(tls connection have some error) {
			close connection direct
		} else {
			if(tls connection already closed) {
				close conenction direct
			} else {
				tear down tls connection 
			}
		}
	}
    */

	c->fclose = 1;

	if(c->ssl) {
		if(c->ssl->f_err || c->ssl->f_closed)  ///ssl con err or ssl closed
			return net_free_direct(c);

		int rc = ssl_shutdown(c);
		if(rc == -11) {	///again
			tm_add(c, net_free_direct, NET_TMOUT);
			return -11;
		}
		///success or error
		return net_free_direct(c);
	}
	return net_free_direct(c);
}

void net_exp(void * data)
{	
	con_t * c = data;
	net_free(c);
}

int net_alloc(con_t ** c)
{
    con_t * nc = NULL;
    schk(nc = mem_pool_alloc(sizeof(con_t)), return -1);
    schk(0 == ev_alloc(&nc->ev), {mem_pool_free(nc); return -1;});
    nc->ev->c = nc;
    *c = nc;
    return 0;
}

int net_init(void)
{
    return 0;
}

int net_end(void)
{
    return 0;
}

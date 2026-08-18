#include "common.h"
#include "net_event.h"
#include "net_event_timer.h"
#include "net_ssl.h"

static int net_io_recv_udp(con_t *c, uint8_t *buf, uint32_t bufn) {
    socklen_t socklen = sizeof(struct sockaddr);
    struct sockaddr_in addr = {0};

    sassert(c != NULL);
    sassert(buf != NULL);
    sassert(bufn > 0);

    for (;;) {
        int recvd = recvfrom(c->fd, buf, bufn, 0, (struct sockaddr *)&addr, &socklen);
        if (recvd <= 0) {
            if (recvd == 0) {
                return 0;
            } else {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    return -11;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    return -1;
                }
            }
        }
        return recvd;
    };
}

static int net_io_send_udp(con_t *c, uint8_t *buf, uint32_t bufn) {
    socklen_t socklen = sizeof(struct sockaddr);

    sassert(c != NULL);
    sassert(buf != NULL);
    sassert(bufn > 0);

    for (;;) {
        int sendn = sendto(c->fd, buf, bufn, 0, (struct sockaddr *)&c->addr, socklen);
        if (sendn < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return -11;
            } else if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }
        return sendn;
    };
}

static int net_io_recv(con_t *c, uint8_t *buf, uint32_t bufn) {
    int rc;
    sassert(bufn > 0);
    sassert(buf != NULL);
    sassert(c != NULL);

    for (;;) {
        rc = recv(c->fd, buf, bufn, 0);
        if (rc <= 0) {
            if (rc == 0) {
                ///err("peer closed\n");
                return -1;
            } else {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    return -11;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    if (errno == ECONNRESET) {
                        ///do not print error message. handle them silently
                        return -1;
                    } 
                    err("net recv err. [%d] [%s]\n", errno, strerror(errno));
                    return -1;
                }
            }
        }
        return rc;
    };
}

static int net_io_send(con_t *c, uint8_t *buf, uint32_t bufn) {
    int rc;

    sassert(bufn > 0);
    sassert(buf != NULL);
    sassert(c != NULL);

    for (;;) {
        rc = send(c->fd, buf, bufn, 0);
        if (rc < 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return -11;
            } else if (errno == EINTR) {
                continue;
            } else {
                err("net send err. [%d] [%s]\n", errno, strerror(errno));
                return -1;
            }
        }
        return rc;
    };
}

static int net_io_send_chain(con_t *c, meta_t *head) {
    sassert(c != NULL);
    sassert(head != NULL);

    for (;;) {
        meta_t *n = head;
        while (n) {
            if (meta_getlen(n) > 0) {
                break;
            }
            n = n->next;
        }
        if (!n) {
            return 1;
        }
        int sendn = net_io_send(c, n->pos, meta_getlen(n));
        if (sendn < 0) {
            if (-11 == sendn) {
                return -11;
            }
            return -1;
        }
        n->pos += sendn;
    }
}

int net_socket_nbio(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    schk(flags != -1, return -1);
    schk(fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1, return -1);
    return 0;
}

int net_socket_reuseport(int fd) {
    int flag = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
        (const void *)&flag, sizeof(int));
}

int net_socket_reuseaddr(int fd) {
    int flag = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
        (const void *)&flag, sizeof(int));
}

int net_socket_fastopen(int fd) {
#if 0
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN,
        (const void *) &flag, sizeof(int));
#endif
    return 0;
}

int net_socket_nodelay(int fd) {
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
        (const void *)&flag, sizeof(int));
}

int net_connect_chk(con_t *c) {
    int errcode = 0;
    socklen_t errn = sizeof(int);
    schk(0 == getsockopt(c->fd, SOL_SOCKET, SO_ERROR, (void *)&errcode, &errn), return -1);
    if (errcode != 0) {
        err("net socket getsockopt(SO_ERROR) err. [%d] [%s]\n", errcode, strerror(errcode));
        return -1;
    }
    return 0;
}

int net_connect(con_t *c, struct sockaddr_in *addr, uint8_t ftcp) {
    int fd = socket(AF_INET, ftcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    schk(fd != -1, return -1);
    schk(net_socket_nbio(fd) == 0, {
        close(fd);
        return -1;
    });
    schk(net_socket_reuseaddr(fd) == 0, {
        close(fd);
        return -1;
    });
    if (ftcp) {
        schk(net_socket_fastopen(fd) == 0, {
            close(fd);
            return -1;
        });
        schk(net_socket_nodelay(fd) == 0, {
            close(fd);
            return -1;
        });
    }
    
    c->fd = fd;
    if (addr) memcpy(&c->addr, addr, sizeof(c->addr));
    if (ftcp) {
        c->send = net_io_send;
        c->recv = net_io_recv;
        c->send_chain = net_io_send_chain;
    } else {
        c->send = net_io_send_udp;
        c->recv = net_io_recv_udp;
        return 0;
    }

    uint32_t mask = 0;
    if (c->read_cb) mask |= EV_R;
    if (c->write_cb) mask |= EV_W;
    if (!mask) {
        err("net connect. but read and write cb all empty\n");
        close(fd);
        return -1;
    }
    net_ev_set(c, mask);

    for (;;) {
        int rc = connect(fd, (struct sockaddr *)&c->addr, sizeof(struct sockaddr_in));
        if (rc != 0) {
            if (errno == EINTR) { /// irq by signal
                continue;
            } else if ((errno == EAGAIN) || (errno == EALREADY) ||
                       (errno == EINPROGRESS)) {
                return -11;
            }
            err("net connect syscall err. [%d]\n", errno);
            return -1;
        }
        return 0;
    }
}

static int net_accept(con_t *c) {

    for (;;) {
        struct sockaddr_in caddr;
        socklen_t caddrn = sizeof(struct sockaddr_in);
        memset(&caddr, 0x0, caddrn);
        
        int new_fd = accept(c->fd, (struct sockaddr *)&caddr, &caddrn);
        if (-1 == new_fd) {
            if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR ||
                errno == EPROTO || errno == ECONNABORTED) {
                return -11;
            }
            err("net accept syscall err. [%d]\n", errno);
            return -1;
        }

        schk(net_socket_nbio(new_fd) == 0, {
            close(new_fd);
            return -1;
        });
        schk(net_socket_nodelay(new_fd) == 0, {
            close(new_fd);
            return -1;
        });
        schk(net_socket_fastopen(new_fd) == 0, {
            close(new_fd);
            return -1;
        });

        con_t *new_con = NULL;
        schk(net_alloc(&new_con) != -1, {
            close(new_fd);
            return -1;
        });
        memcpy(&new_con->addr, &caddr, caddrn);
        new_con->fd = new_fd;
        new_con->recv = net_io_recv;
        new_con->send = net_io_send;
        new_con->send_chain = net_io_send_chain;
        new_con->fssl = c->fssl;

        new_con->read_cb = (net_ev_cb)c->user_data;
        new_con->write_cb = NULL;
        net_ev_set(new_con, EV_R);
        net_timer_add(new_con, net_free_timeout, NET_TMOUT);
    }
    return 0;
}

int net_listen(net_ev_cb cb, struct sockaddr_in *addr, uint8_t fssl) {
    con_t *c = NULL;
    do {
        schk(0 == net_alloc(&c), break); 
    
        c->fd = socket(AF_INET, SOCK_STREAM, 0);
        schk(c->fd != -1, break);
        schk(net_socket_nbio(c->fd) == 0, break);
        schk(net_socket_reuseaddr(c->fd) == 0, break);
        schk(net_socket_reuseport(c->fd) == 0, break);
        schk(net_socket_fastopen(c->fd) == 0, break);
        schk(net_socket_nodelay(c->fd) == 0, break);

        schk(0 == bind(c->fd, (struct sockaddr *)addr, sizeof(struct sockaddr)), break);
        schk(0 == listen(c->fd, SOMAXCONN), break);

        c->read_cb = net_accept;
        c->write_cb = NULL;
        c->user_data = (void*)cb;
        c->free_user_data = NULL;
        c->fssl = (fssl ? 1 : 0);
        net_ev_set(c, EV_R);
        return 0;
    } while (0);

    if (c) net_free(c);
    return 0;
}

static void net_free_internal(void *data) {
    con_t *c = data;

    if (c->timer) {
        ev_timer_del(c->timer);
        mem_pool_free(c->timer);
        c->timer = NULL;
    }
    
    if (c->fd > 0) {
        if (c->ev) net_ev_set(c, EV_NONE);
        close(c->fd);
        c->fd = 0;
    }

    if (c->ev) {
        ev_free(c->ev);
        c->ev = NULL;
    }

    if (c->meta) {
        meta_free(c->meta);
        c->meta = NULL;
    }

    if (c->user_data && c->free_user_data) {
        c->free_user_data(c->user_data);
    }

    if (c->ssl) {
        SSL_free(c->ssl->con);
        mem_pool_free(c->ssl);
        c->ssl = NULL;
    }
    mem_pool_free(c);
    return;
}

static void net_free_timeout_ssl_shutdown(ev_timer_t *timer) {
    con_t *c = ev_timer_userdata(timer);
    net_free_internal(c);
}

void net_free_timeout(ev_timer_t *timer) {
    con_t *c = ev_timer_userdata(timer);
    net_free(c);
}

int net_free(con_t *c) {
    if (c->fclosing) return 0;
    c->fclosing = 1;

    if (c->ssl) {
        if (c->ssl->f_closed) {
            net_free_internal(c);
            return 0;
        }

        net_timer_add(c, net_free_timeout_ssl_shutdown, NET_TMOUT);
        int rc = net_ssl_shutdown(c);
        if (rc == -11) {
            return -11;
        }
        
        net_free_internal(c);
        return 0;
    }
    net_free_internal(c);
    return 0;
}


int net_alloc(con_t **c) {
    con_t *new_con = mem_pool_alloc(sizeof(con_t));
    schk(new_con, return -1);

    do {
        schk(0 == ev_alloc(&new_con->ev), break);
        new_con->ev->data = new_con;
        *c = new_con;
        return 0;
    } while (0);

    net_free_internal(new_con);
    return -1;
}

int net_init(void) {
    ev_timer_init();
    ev_init();
    net_ssl_init();

    return 0;
}

int net_exit(void) {
    net_ssl_exit();
    ev_exit();
    ev_timer_exit();

    return 0;
}

void net_ev_loop(void) {
    ev_loop();
    return;
}

void net_ev_set(con_t *c, uint32_t new_mask) {
    ev_opt(c->ev, c->fd, new_mask);
    return;
}

uint32_t net_ev_add(con_t *c, uint32_t mask) {
    c->ev->mask |= mask;
    return c->ev->mask;
}

uint32_t net_ev_clr(con_t *c, uint32_t mask) {
    c->ev->mask &= ~mask;
    return c->ev->mask;
}

int net_timer_add(con_t *c, net_timer_cb cb, uint64_t delay_ms) {
    if (!c->timer) {
        c->timer = mem_pool_alloc(sizeof(ev_timer_t));
        if (!c->timer) {
            err("net timer alloc err\n");
            return -1;
        }
    }
    c->timer->f_once = 1;
    return ev_timer_add(c->timer, cb, c, delay_ms);
}

int net_timer_del(con_t *c) {
    if (c->timer) {
        return ev_timer_del(c->timer);
    }
    return 0;
}

ev_timer_t *ev_timer_alloc(net_timer_cb cb, void *data, uint64_t delay_ms) {
    ev_timer_t *timer = mem_pool_alloc(sizeof(ev_timer_t));
    schk(timer, return NULL);

    schk(0 == ev_timer_add(timer, cb, data, delay_ms), {
        mem_pool_free(timer);
        return NULL;
    })
    return timer;
}

void ev_timer_free(ev_timer_t *timer) {
    ev_timer_del(timer);
    mem_pool_free(timer);
}

void *ev_timer_userdata(ev_timer_t *timer) {
    return timer->user_data;
}



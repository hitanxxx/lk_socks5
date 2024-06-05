#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"


static int s5_local_auth_send(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    int sendn = up->send_chain(up, meta);
    if(sendn < 0) {
        if(sendn == -1) {
            err("s5 local send authorization data failed\n");
            s5_free(s5);
            return -1;
        }
        timer_set_data(&ev->timer, s5);
        timer_set_pt(&ev->timer, s5_timeout_cb);
        timer_add(&ev->timer, S5_TIMEOUT);
        return -11;
    }
    timer_del(&ev->timer);
    meta->pos = meta->last = meta->start;
    s5->down->event->read_pt = s5_traffic_process;
    s5->down->event->write_pt = NULL;
    return s5->down->event->read_pt(s5->down->event);
}

static int s5_local_auth_build(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    ///fill in s5_auth_t
    meta->pos = meta->last = meta->start;
    s5_auth_t * auth = (s5_auth_t*)meta->last;
    auth->magic = htonl(S5_AUTH_LOCAL_MAGIC);
    memset(auth->key, 0, sizeof(auth->key));
    memcpy((char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key));
    meta->last += sizeof(s5_auth_t);
#ifndef S5_OVER_TLS
    if(!s5->cipher_enc) {
        if(0 != sys_cipher_ctx_init(&s5->cipher_enc, 0)) {
            err("s5 local cipher enc init failed\n");
            s5_free(s5);
            return -1;
        }
    }
    if(sizeof(s5_auth_t) != sys_cipher_conv(s5->cipher_enc, meta->pos, sizeof(s5_auth_t))) {
        err("s5 local cipher enc meta failed\n");
        s5_free(s5);
        return -1;
    }
#endif
    ev->write_pt = s5_local_auth_send; /// goto send s5 private authorization login request
    return ev->write_pt(ev);
}

static inline void s5_local_up_addr_get(struct sockaddr_in * addr)
{
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config_get()->s5_local_serv_port);
    addr->sin_addr.s_addr = inet_addr(config_get()->s5_local_serv_ip);
}

static int s5_local_up_connect_ssl(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * cycle = up->data;

    if(!up->ssl->f_handshaked) {
        err("s5 local to s5 server. ssl handshake err\n");
        s5_free(cycle);
        return -1;
    }
    timer_del(&ev->timer);
    
    up->recv = ssl_read;
    up->send = ssl_write;
    up->send_chain = ssl_write_chain;

    ev->write_pt = s5_local_auth_build;
    return ev->write_pt(ev);
}

static int s5_local_up_connect_check(event_t * ev)
{
    con_t* up = ev->data;
    timer_del(&ev->timer);
    
    s5_session_t * s5 = up->data;    
    if(0 != net_socket_check_status(up->fd)) {
        err("s5 local connect check status failed\n");
        s5_free(s5);
        return -1;
    }
    net_socket_nodelay(up->fd);
    
    up->fssl = 1;
#ifndef S5_OVER_TLS
    up->fssl = 0;
#endif    
    if(up->fssl) {
        if(0 != ssl_create_connection(up, L_SSL_CLIENT)) {
            err("s5 local create ssl connection for up failed\n");
            s5_free(s5);
            return -1;
        }
        int rc = ssl_handshake(up->ssl);
        if(rc < 0) {
            if(rc == -11) {
                up->ssl->cb = s5_local_up_connect_ssl;
                timer_set_data(&ev->timer, s5);
                timer_set_pt(&ev->timer, s5_timeout_cb);
                timer_add(&ev->timer, S5_TIMEOUT);
                return -11;
            }
            err("s5 local ssl handshake failed\n");
            s5_free(s5);
            return -1;
        }
        return s5_local_up_connect_ssl(ev);
    }
    ev->write_pt = s5_local_auth_build;
    return ev->write_pt(ev);
}

static int s5_local_down_recv(event_t * ev)
{
    /// cache read data
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    meta_t * meta = down->meta;
    int readn = 0;

    for(;;) {
        /// check meta remain space
        if(meta->end <= meta->last) {
            err("s5 local down recv cache data too much\n");
            s5_free(s5);
            return -1;
        }

        /// cache read data
        readn = down->recv(down, meta->last, meta->end - meta->last);
        if(readn < 0) {
            if(readn == -11) {
                timer_set_data(&s5->up->event->timer, s5);
                timer_set_pt(&s5->up->event->timer, s5_timeout_cb);
                timer_add(&s5->up->event->timer, S5_TIMEOUT);
                return -11;
            }
            err("s5 local down recv failed\n");
            s5_free(s5);
            return -1;
        }
        meta->last += readn;
    }
}

int s5_local_accept_cb(event_t * ev)
{
    con_t * down = ev->data;
    if(!down->meta) {
        if(0 != meta_alloc(&down->meta, 8192)) {
            err("s5 local alloc down meta failed\n");
            net_free(down);
            return -1;
        }
    }
    down->event->read_pt = s5_local_down_recv;
    down->event->write_pt = NULL;
    event_opt(down->event, down->fd, EV_R);
    
    s5_session_t * s5 = NULL;
    if(0 != s5_alloc(&s5)) {
        err("s5 cycle alloc failed\n");
        net_free(down);
        return -1;
    }
    s5->typ = SOCKS5_CLIENT;
    s5->down = down;
    down->data = s5;
    if(0 != net_alloc(&s5->up)) {
        err("s5 up alloc failed\n");
        s5_free(s5);
        return -1;
    }
    s5->up->data = s5;
    if(!s5->up->meta) {
        if(0 != meta_alloc(&s5->up->meta, 8192)) {
            err("s5 up meta alloc failed\n");
            s5_free(s5);
            return -1;
        }
    }
    s5->up->event->read_pt = NULL;
    s5->up->event->write_pt	= s5_local_up_connect_check;
    s5_local_up_addr_get(&s5->up->addr);
    int rc = net_connect(s5->up, &s5->up->addr);
    if(rc < 0) {
        if(rc == -11) {
            if(s5->up->event->opt != EV_W) event_opt(s5->up->event, s5->up->fd, EV_W);
            timer_set_data(&s5->up->event->timer, s5);
            timer_set_pt(&s5->up->event->timer, s5_timeout_cb);
            timer_add(&s5->up->event->timer, S5_TIMEOUT);
            return -11;
        }
        err("cycle up connect failed\n");
        s5_free(s5);
        return -1;
    }
    return s5->up->event->write_pt(s5->up->event);
}

int socks5_local_init(void)
{
    return OK;
}

int socks5_local_end(void)
{
    return OK;
}



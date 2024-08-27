#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

static inline void s5_loc_get_upaddr(struct sockaddr_in * addr)
{
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config_get()->s5_local_serv_port);
    addr->sin_addr.s_addr = inet_addr(config_get()->s5_local_serv_ip);
}

static int s5_loc_auth_send(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    int sendn = up->send_chain(up, meta);
    if(sendn < 0) {
        if(sendn == -1) {
            err("s5 local auth req send err\n");
            s5_free(s5);
            return -1;
        }
        timer_set_data(&ev->timer, s5);
        timer_set_pt(&ev->timer, s5_timeout_cb);
        timer_add(&ev->timer, S5_TIMEOUT);
        return -11;
    }
    timer_del(&ev->timer);
    meta_clr(meta);
    s5->down->event->read_pt = s5_traffic_process;
    s5->down->event->write_pt = NULL;
    return s5->down->event->read_pt(s5->down->event);
}

static int s5_loc_auth_build(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * s5 = up->data;
    meta_t * meta = s5->up->meta;

    ///build s5 auth req
    meta_clr(meta);
    s5_auth_t * auth = (s5_auth_t*)meta->last;
    auth->magic = htonl(S5_AUTH_MAGIC);
    memset(auth->secret, 0, sizeof(auth->secret));
    memset(auth->key, 0, sizeof(auth->key));
    memcpy((char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key));
    meta->last += sizeof(s5_auth_t);

    ev->write_pt = s5_loc_auth_send; ///goto send s5 private authorization login request
    return ev->write_pt(ev);
}


static int s5_loc_connect_chk_ssl(event_t * ev)
{
    con_t* up = ev->data;
    s5_session_t * cycle = up->data;

    if(!up->ssl->f_handshaked) {
        err("s5 local -> s5 server. ssl handshake err\n");
        s5_free(cycle);
        return -1;
    }
    timer_del(&ev->timer);
    
    up->recv = ssl_read;
    up->send = ssl_write;
    up->send_chain = ssl_write_chain;

    ev->write_pt = s5_loc_auth_build;
    return ev->write_pt(ev);
}

static int s5_loc_connect_chk(event_t * ev)
{
    con_t* up = ev->data;
    timer_del(&ev->timer);
    
    s5_session_t * s5 = up->data;   
    schk(net_socket_check_status(up->fd) == 0, {s5_free(s5);return -1;});
    net_socket_nodelay(up->fd);
    
    up->fssl = 1;  
    if(up->fssl) {
        schk(ssl_create_connection(up, L_SSL_CLIENT) == 0, {s5_free(s5);return -1;});
        int rc = ssl_handshake(up->ssl);
        if(rc < 0) {
            if(rc == -11) {
                up->ssl->cb = s5_loc_connect_chk_ssl;
                timer_set_data(&ev->timer, s5);
                timer_set_pt(&ev->timer, s5_timeout_cb);
                timer_add(&ev->timer, S5_TIMEOUT);
                return -11;
            }
            err("s5 local ssl handshake failed\n");
            s5_free(s5);
            return -1;
        }
        return s5_loc_connect_chk_ssl(ev);
    }
    ev->write_pt = s5_loc_auth_build;
    return ev->write_pt(ev);
}

static int s5_loc_down_recv(event_t * ev) {
    /// cache read data
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    meta_t * meta = down->meta;
    int readn = 0;

    for(;;) {
        if(meta_getfree(meta) < 1) {
            err("s5 local down recv cache data too much\n");
            s5_free(s5);
            return -1;
        }

        /// cache read data
        readn = down->recv(down, meta->last, meta_getfree(meta));
        if(readn < 0) {
            if(readn == -11) {
                timer_set_data(&s5->up->event->timer, s5);
                timer_set_pt(&s5->up->event->timer, s5_timeout_cb);
                timer_add(&s5->up->event->timer, S5_TIMEOUT);
                return -11;
            }
            err("s5 local down recv err\n");
            s5_free(s5);
            return -1;
        }
        meta->last += readn;
    }
}

int s5_loc_accept(event_t * ev)
{
    con_t * down = ev->data;

    if(!down->meta) {
        if(0 != meta_alloc(&down->meta, S5_METAN)) {
            err("alloc down meta err\n");
            net_free(down);
            return -1;
        }
    }
    down->event->read_pt = s5_loc_down_recv;
    down->event->write_pt = NULL;
    event_opt(down->event, down->fd, EV_R);

    s5_session_t * s5 = NULL;
    if(0 != s5_alloc(&s5)) {
        err("alloc s5 err\n");
        net_free(down);
        return -1;
    }
    s5->typ = SOCKS5_CLIENT;
    s5->down = down;
    down->data = s5;
    if(0 != net_alloc(&s5->up)) {
        err("alloc s5 up err\n");
        s5_free(s5);
        return -1;
    }
    s5->up->data = s5;
    if(NULL == s5->up->meta) {
        if(0 != meta_alloc(&s5->up->meta, S5_METAN)) {
            err("alloc s5 up meta err\n");
            s5_free(s5);
            return -1;
        }
    }
    s5->up->event->read_pt = NULL;
    s5->up->event->write_pt = s5_loc_connect_chk;
    
    s5_loc_get_upaddr(&s5->up->addr);
    int rc = net_connect(s5->up, &s5->up->addr);
    if(rc < 0) {
        if(rc == -11) {
            if(s5->up->event->opt != EV_W) event_opt(s5->up->event, s5->up->fd, EV_W);
            timer_set_data(&s5->up->event->timer, s5);
            timer_set_pt(&s5->up->event->timer, s5_timeout_cb);
            timer_add(&s5->up->event->timer, S5_TIMEOUT);
            return -11;
        }
        err("s5 up connect failed\n");
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



#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"

static inline void tls_tunnel_s_addr(struct sockaddr_in * addr)
{
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config_get()->s5_local_serv_port);
    addr->sin_addr.s_addr = inet_addr(config_get()->s5_local_serv_ip);
}

static int tls_tunnel_c_auth_send(event_t * ev)
{
    con_t* cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;
    meta_t * meta = ses->cup->meta;

    int sendn = cup->send_chain(cup, meta);
    if(sendn < 0) {
        if(sendn == -1) {
            err("TLS tunnel auth req send err\n");
            tls_ses_free(ses);
            return -1;
        }
        timer_set_data(&ev->timer, ses);
        timer_set_pt(&ev->timer, tls_session_timeout);
        timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
        return -11;
    }
    timer_del(&ev->timer);
    meta_clr(meta);
    ses->cdown->event->read_pt = tls_tunnel_traffic_proc;
    ses->cdown->event->write_pt = NULL;
    return ses->cdown->event->read_pt(ses->cdown->event);
}

static int tls_tunnel_c_auth_build(event_t * ev)
{
    con_t* cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;
    meta_t * meta = ses->cup->meta;

    ///build auth req
    meta_clr(meta);
    tls_tunnel_auth_t * auth = (tls_tunnel_auth_t*)meta->last;
    auth->magic = htonl(TLS_TUNNEL_AUTH_MAGIC_NUM);
    memset(auth->secret, 0, sizeof(auth->secret));
    memset(auth->key, 0, sizeof(auth->key));
    memcpy((char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key));
    meta->last += sizeof(tls_tunnel_auth_t);

    ev->write_pt = tls_tunnel_c_auth_send;
    return ev->write_pt(ev);
}

static int tls_tunnel_c_connect_chk_tls(event_t * ev)
{
    con_t* cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;

    if(!cup->ssl->f_handshaked) {
        err("TLS tunnel. handshake err\n");
        tls_ses_free(ses);
        return -1;
    }
    timer_del(&ev->timer);
    
    cup->recv = ssl_read;
    cup->send = ssl_write;
    cup->send_chain = ssl_write_chain;

    ev->write_pt = tls_tunnel_c_auth_build;
    return ev->write_pt(ev);
}

static int tls_tunnel_c_connect_chk(event_t * ev)
{
    con_t* cup = ev->data;
    timer_del(&ev->timer);
    
    tls_tunnel_session_t * ses = cup->data;   
    schk(net_socket_check_status(cup->fd) == 0, {tls_ses_free(ses);return -1;});
    net_socket_nodelay(cup->fd);
    
    cup->fssl = 1;
    if(cup->fssl) {
        schk(ssl_create_connection(cup, L_SSL_CLIENT) == 0, {tls_ses_free(ses);return -1;});
        int rc = ssl_handshake(cup->ssl);
        if(rc < 0) {
            if(rc == -11) {
                cup->ssl->cb = tls_tunnel_c_connect_chk_tls;
                timer_set_data(&ev->timer, ses);
                timer_set_pt(&ev->timer, tls_session_timeout);
                timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshake failed\n");
            tls_ses_free(ses);
            return -1;
        }
        return tls_tunnel_c_connect_chk_tls(ev);
    }
    ev->write_pt = tls_tunnel_c_auth_build;
    return ev->write_pt(ev);
}

static int tls_tunnel_c_recv(event_t * ev) {
    /// cache read data
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    meta_t * meta = cdown->meta;
    int readn = 0;

    for(;;) {
        if(meta_getfree(meta) < 1) {
            err("TLS tunnel cdown recv ccdata too much\n");
            tls_ses_free(ses);
            return -1;
        }

        /// cache read data
        readn = cdown->recv(cdown, meta->last, meta_getfree(meta));
        if(readn < 0) {
            if(readn == -11) {
                timer_set_data(&ses->cup->event->timer, ses);
                timer_set_pt(&ses->cup->event->timer, tls_session_timeout);
                timer_add(&ses->cup->event->timer, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel cdown recv err\n");
            tls_ses_free(ses);
            return -1;
        }
        meta->last += readn;
    }
}

int tls_tunnel_c_accept(event_t * ev)
{
    con_t * cdown = ev->data;
    if(!cdown->meta) {
        if(0 != meta_alloc(&cdown->meta, TLS_TUNNEL_METAN)) {
            err("alloc cdown meta err\n");
            net_free(cdown);
            return -1;
        }
    }
    cdown->event->read_pt = tls_tunnel_c_recv;
    cdown->event->write_pt = NULL;
    event_opt(cdown->event, cdown->fd, EV_R);
    

    tls_tunnel_session_t * ses = NULL;
    if(0 != tls_ses_alloc(&ses)) {
        err("alloc TLS tunnel err\n");
        net_free(cdown);
        return -1;
    }
    ses->typ = TLS_TUNNEL_C;
    ses->cdown = cdown;
    cdown->data = ses;
    if(0 != net_alloc(&ses->cup)) {
        err("alloc TLS tunnel cup err\n");
        tls_ses_free(ses);
        return -1;
    }
    ses->cup->data = ses;
    if(!ses->cup->meta) {
        if(0 != meta_alloc(&ses->cup->meta, TLS_TUNNEL_METAN)) {
            err("alloc TLS tunnel cup meta err\n");
            tls_ses_free(ses);
            return -1;
        }
    }
    ses->cup->event->read_pt = NULL;
    ses->cup->event->write_pt = tls_tunnel_c_connect_chk;
    
    tls_tunnel_s_addr(&ses->cup->addr);
    int rc = net_connect(ses->cup, &ses->cup->addr);
    if(rc < 0) {
        if(rc == -11) {
            if(ses->cup->event->opt != EV_W)
                event_opt(ses->cup->event, ses->cup->fd, EV_W);
            
            timer_set_data(&ses->cup->event->timer, ses);
            timer_set_pt(&ses->cup->event->timer, tls_session_timeout);
            timer_add(&ses->cup->event->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel cup connect failed\n");
        tls_ses_free(ses);
        return -1;
    }
    return ses->cup->event->write_pt(ses->cup->event);
}

int tls_tunnel_c_init(void)
{
    return OK;
}

int tls_tunnel_c_exit(void)
{
    return OK;
}



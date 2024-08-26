#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

#define S5_USER_AUTH_FILE_LEN  (4*1024)

typedef struct {
    ezac_ctx_t * ac;    
} g_s5_t;
static g_s5_t * g_s5_ctx = NULL;

static int s5_srv_try_read(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;

    schk(0 == net_socket_check_status(down->fd), {s5_free(s5); return -1;});
    return 0;
}


int s5_alloc(s5_session_t ** s5)
{
    s5_session_t * ns5 = NULL;
    schk(ns5 = mem_pool_alloc(sizeof(s5_session_t)), return -1);
    *s5 = ns5;
    return 0;
}

int s5_free(s5_session_t * s5)
{
    memset(&s5->phase1, 0x0, sizeof(s5_rfc_phase1_req_t));
    memset(&s5->phase2, 0x0, sizeof(s5_rfc_phase2_req_t));
    if(s5->down) {
        net_free(s5->down);
        s5->down = NULL;
    }
    if(s5->up) {
        net_free(s5->up);
        s5->up = NULL;
    }
    if(s5->dns_session) {
        dns_over(s5->dns_session);
        s5->dns_session = NULL;
    }

    s5->state = 0;
    s5->frecv_err_down = 0;
    s5->frecv_err_up = 0;

    mem_pool_free(s5);
    return 0;
}

void s5_timeout_cb(void * data)
{
    s5_free((s5_session_t *)data);
}

static int s5_traffic_recv(event_t * ev)
{
    con_t * c = ev->data;
    s5_session_t * s5 = c->data;
    con_t * down = s5->down;
    con_t * up = s5->up;    
    int recvn = 0;

    timer_set_data(&ev->timer, (void*)s5);
    timer_set_pt(&ev->timer, s5_timeout_cb);
    timer_add(&ev->timer, S5_TIMEOUT);

    while(meta_getfree(down->meta) > 0) {
        recvn = down->recv(down, down->meta->last, meta_getfree(down->meta));
        if(recvn < 0) {
            if(recvn == -1) {
                err("s5 down recv error\n");
                s5->frecv_err_down = 1;
            }
            break; ///until again
        }
        down->meta->last += recvn;
    }

    if(meta_getlen(down->meta) > 0) {
        event_opt(down->event, down->fd, down->event->opt & (~EV_R));
        event_opt(up->event, up->fd, up->event->opt|EV_W);
        return up->event->write_pt(up->event);
    }
    if(s5->frecv_err_down) {
        s5_free(s5);
        return -1;
    }
    return -11;
}

static int s5_traffic_send(event_t * ev)
{	
    con_t * c = ev->data;
    s5_session_t * s5 = c->data;
    con_t * down = s5->down;
    con_t * up = s5->up;
    int sendn = 0;

    timer_set_data(&ev->timer, (void*)s5);
    timer_set_pt(&ev->timer, s5_timeout_cb);
    timer_add(&ev->timer, S5_TIMEOUT);

    while(meta_getlen(down->meta) > 0) {
        sendn = up->send(up, down->meta->pos, meta_getlen(down->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("s5 up send error\n");
                s5_free(s5);
                return -1;
            }
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        down->meta->pos += sendn;
    }

    if(s5->frecv_err_down) {
        err("s5 up send, down already error\n");
        s5_free(s5);
        return -1;
    }
    meta_clr(down->meta);
    event_opt(up->event, up->fd, up->event->opt & (~EV_W));
    event_opt(down->event, down->fd, down->event->opt|EV_R);
    return down->event->read_pt(down->event);
}

static int s5_traffic_back_recv(event_t * ev)
{
    con_t * c = ev->data;
    s5_session_t * s5 = c->data;
    con_t * down = s5->down;
    con_t * up = s5->up;
    int recvn = 0;

    timer_set_data(&ev->timer, (void*)s5);
    timer_set_pt(&ev->timer, s5_timeout_cb);
    timer_add(&ev->timer, S5_TIMEOUT);

    while(meta_getfree(up->meta) > 0) {
        recvn = up->recv(up, up->meta->last, meta_getfree(up->meta));
        if(recvn < 0) {
            if(recvn == -1) {   
                err("s5 up recv error\n");
                s5->frecv_err_up = 1;
            }
            break;
        }
        up->meta->last += recvn;
    }

    if(meta_getlen(up->meta) > 0) {
        event_opt(up->event, up->fd, up->event->opt & (~EV_R));
        event_opt(down->event, down->fd, down->event->opt|EV_W);
        return down->event->write_pt(down->event);
    }
    if(s5->frecv_err_up) {
        s5_free(s5);
        return -1;
    }
    return -11;
}

static int s5_traffic_back_send(event_t * ev)
{
    con_t * c = ev->data;
    s5_session_t * s5 = c->data;
    con_t * down = s5->down;
    con_t * up = s5->up;
    int sendn = 0;
    
    timer_set_data(&ev->timer, (void*)s5);
    timer_set_pt(&ev->timer, s5_timeout_cb);
    timer_add(&ev->timer, S5_TIMEOUT);

    while(meta_getlen(up->meta) > 0) {
        sendn = down->send(down, up->meta->pos, meta_getlen(up->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("s5 down send error\n");
                s5_free(s5);
                return -1;
            }
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        up->meta->pos += sendn;
    }
    if(s5->frecv_err_up) {
        err("s5 down send, up already error\n");
        s5_free(s5);
        return -1;
    }
    meta_clr(up->meta);
    event_opt(down->event, down->fd, down->event->opt & (~EV_W));
    event_opt(up->event, up->fd, up->event->opt|EV_R);
    return up->event->read_pt(up->event);
}

int s5_traffic_process(event_t * ev)
{
    con_t * c = ev->data;
    s5_session_t * s5 = c->data;
    con_t * down = s5->down;
    con_t * up = s5->up;

    if(!down->meta) {
        schk(meta_alloc(&down->meta, S5_METAN) == 0, {s5_free(s5);return -1;});
    }
    if(!up->meta) {
        schk(meta_alloc(&up->meta, S5_METAN) == 0, {s5_free(s5);return -1;});
    }
    
    ///only clear up meta in here. because s5 local run in here too.
    ///local(down) mabey recv some data.
    meta_clr(up->meta);

    ///down->up
    s5->down->event->read_pt = s5_traffic_recv;
    s5->up->event->write_pt	= s5_traffic_send;

    ///up->down
    s5->up->event->read_pt = s5_traffic_back_recv;
    s5->down->event->write_pt = s5_traffic_back_send;
    
    event_opt(s5->up->event, s5->up->fd, EV_R);	
    event_opt(s5->down->event, s5->down->fd, EV_R);
    return s5->down->event->read_pt(s5->down->event);
}

static int s5_srv_ph2_ack(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    int rc = 0;
    meta_t * meta = down->meta;

    while(meta_getlen(meta) > 0) {
        rc = down->send(down, meta->pos, meta_getlen(meta));
        if(rc < 0) {
            if(rc == -1) {
                err("s5 server adv resp send failed\n");
                s5_free(s5);
                return -1;
            }
            timer_set_data(&ev->timer, down);
            timer_set_pt(&ev->timer, s5_timeout_cb);
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        meta->pos += rc;
    }
    timer_del(&ev->timer);
    
    meta->pos = meta->last = meta->start;  /// meta clear 
    /// goto process local server transfer data
    ev->read_pt = s5_traffic_process;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

static int s5_srv_remote_connect_chk(event_t * ev)
{
    con_t * up = ev->data;
    s5_session_t * s5 = up->data;
    con_t * down = s5->down;
    meta_t * meta = down->meta;

    schk(net_socket_check_status(up->fd) == 0, {s5_free(s5);return -1;});
    net_socket_nodelay(up->fd);
    timer_del(&ev->timer);

    s5_rfc_phase2_resp_t * resp = (s5_rfc_phase2_resp_t*)meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)up->addr.sin_addr.s_addr);
    resp->bnd_port = htons(up->addr.sin_port);
    meta->last += sizeof(s5_rfc_phase2_resp_t);

    event_opt(up->event, up->fd, EV_NONE);
    event_opt(down->event, down->fd, EV_W);    
    down->event->write_pt = s5_srv_ph2_ack;
    return down->event->write_pt(down->event);
}

static int s5_srv_remote_connect(event_t * ev)
{
    con_t * up = ev->data;
    s5_session_t * s5 = up->data;
    status rc = 0;

    rc = net_connect(s5->up, &s5->up->addr);
    schk(rc != -1, {s5_free(s5);return -1;});
    ev->read_pt = NULL;
    ev->write_pt = s5_srv_remote_connect_chk;
    event_opt(ev, up->fd, EV_W);
    if(rc == -11) {
        timer_set_data(&ev->timer, s5);
        timer_set_pt(&ev->timer, s5_timeout_cb);
        timer_add(&ev->timer, S5_TIMEOUT);
        return -11;
    }
    return ev->write_pt(ev);
}



static void s5_srv_remote_addr_cb(void * data)
{
    s5_session_t * s5 = data;
    dns_cycle_t * dns_cycle = s5->dns_session;
    char ipstr[128] = {0};

    if(dns_cycle) {
        if(0 == dns_cycle->dns_status) {
            uint16_t addr_port = 0;
            memcpy(&addr_port, s5->phase2.dst_port, sizeof(uint16_t)); 
        
            snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                dns_cycle->answer.answer_addr[0],
                dns_cycle->answer.answer_addr[1],
                dns_cycle->answer.answer_addr[2],
                dns_cycle->answer.answer_addr[3]);
            
            s5->up->addr.sin_family	= AF_INET;
            s5->up->addr.sin_port = addr_port;
            s5->up->addr.sin_addr.s_addr = inet_addr(ipstr);
            
            s5->up->event->read_pt = NULL;
            s5->up->event->write_pt = s5_srv_remote_connect;
            s5->up->event->write_pt(s5->up->event);
        } else {
            err("socks5 server dns resolv failed\n");
            s5_free(s5);
        }
    }
}

static int s5_srv_remote_init(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    char ipstr[128] = {0};
    status rc = 0;

    /// s5 rfc phase2 resp not send yet. down just check connection
    down->event->read_pt = s5_srv_try_read;
    down->event->write_pt = NULL;
    event_opt(down->event, down->fd, EV_R);

    /// alloc upstream connection
    schk(net_alloc(&s5->up) == 0, {s5_free(s5);return -1;});
    s5->up->data = s5;

    if(s5->phase2.atyp == S5_RFC_IPV4) {  /// ipv4 type request, goto convert ipv4 address
        uint16_t addr_port = 0;
        memcpy(&addr_port, s5->phase2.dst_port, sizeof(uint16_t));
        snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
            (unsigned char )s5->phase2.dst_addr[0],
            (unsigned char )s5->phase2.dst_addr[1],
            (unsigned char )s5->phase2.dst_addr[2],
            (unsigned char )s5->phase2.dst_addr[3]);
        
        s5->up->addr.sin_family	= AF_INET;
        s5->up->addr.sin_port = addr_port;
        s5->up->addr.sin_addr.s_addr = inet_addr(ipstr);

        s5->up->event->read_pt = NULL;
        s5->up->event->write_pt = s5_srv_remote_connect;
        return s5->up->event->write_pt(s5->up->event);
    }  else if (s5->phase2.atyp == S5_RFC_DOMAIN) {  ///domain type request, goto dns resolve the domain
        if(0 == dns_rec_find((char*)s5->phase2.dst_addr, ipstr)) {  /// dns cache find success, use dns cache  
            uint16_t addr_port = 0;
            memcpy(&addr_port, s5->phase2.dst_port, sizeof(uint16_t));
            snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                (unsigned char )ipstr[0],
                (unsigned char )ipstr[1],
                (unsigned char )ipstr[2],
                (unsigned char )ipstr[3]);
            s5->up->addr.sin_family = AF_INET;
            s5->up->addr.sin_port = addr_port;
            s5->up->addr.sin_addr.s_addr = inet_addr(ipstr);

            s5->up->event->read_pt = NULL;
            s5->up->event->write_pt = s5_srv_remote_connect;
            return s5->up->event->write_pt(s5->up->event);
        } else {  /// dns cache find failed, goto dns query
            rc = dns_create(&s5->dns_session);
            schk(rc != -1, {s5_free(s5);return -1;});
            strncpy((char*)s5->dns_session->query, (char*)s5->phase2.dst_addr, sizeof(s5->dns_session->query));
            s5->dns_session->cb = s5_srv_remote_addr_cb;
            s5->dns_session->cb_data = s5;
            return dns_start(s5->dns_session);
        }
    }
    err("s5 server phase2 atyp [0x%x]. not support\n", s5->phase2.atyp);
    s5_free(s5);
    return -1;
}

static int s5_srv_ph2_req(event_t * ev)
{
    unsigned char * p = NULL;
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    meta_t * meta = down->meta;
    enum {
        VER = 0,
        CMD,
        RSV,
        TYP,
        TYP_V4,
        TYP_V6,
        TYP_DOMAINN,
        TYP_DOMAIN,
        PORT,
        END
    };

    /*
    s5 msg phase2 format
        char  char  char  char   ...    char*2
        VER | CMD | RSV | ATYP | ADDR | PORT
    */

    for(;;) {
        if(meta_getlen(meta) < 1) {  ///try recv
            int recvn = down->recv(down, meta->last, meta_getfree(meta));
            if(recvn < 0) {
                if(recvn == -1) {
                    err("s5 server rfc phase2 recv failed\n");
                    s5_free(s5);
                    return -1;
                }
                timer_set_data(&ev->timer, (void*)s5);
                timer_set_pt(&ev->timer, s5_timeout_cb);
                timer_add(&ev->timer, S5_TIMEOUT);
                return -11;
            }
            meta->last += recvn;
        }
        
        for(; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if(s5->state == VER) {
                /// ver is fixed. 0x05
                s5->phase2.ver = *p;
                s5->state = CMD;
                continue;
            }
            if(s5->state == CMD) {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5->phase2.cmd = *p;
                s5->state = RSV;
                continue;
            }
            if(s5->state == RSV) {    // RSV means resverd
                s5->phase2.rsv = *p;
                s5->state = TYP;
                continue;
            }
            if(s5->state == TYP) {
                s5->phase2.atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet of domain part
                    0x04		ipv6		16
                */
                if(s5->phase2.atyp == S5_RFC_IPV4) {
                    s5->state = TYP_V4;
                    s5->phase2.dst_addr_n = 4;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if (s5->phase2.atyp == S5_RFC_IPV6) {
                    s5->state = TYP_V6;
                    s5->phase2.dst_addr_n = 16;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if (s5->phase2.atyp == S5_RFC_DOMAIN) {
                    /// atpy domain -> dst addr domain len -> dst addr domain
                    s5->state = TYP_DOMAINN;
                    s5->phase2.dst_addr_n = 0;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                }
                err("s5 server request atyp [%d] not support\n", s5->phase2.atyp);
                s5_free(s5);
                return -1;
            }
            if(s5->state == TYP_V4) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if(s5->phase2.dst_addr_cnt == 4) {
                    s5->state = PORT;
                    continue;
                }
            }
            if(s5->state == TYP_V6) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if(s5->phase2.dst_addr_cnt == 16) {
                    s5->state = PORT;
                    continue;
                }
            }
            if(s5->state == TYP_DOMAINN) {
                s5->phase2.dst_addr_n = *p;
                s5->state = TYP_DOMAIN;
                if(s5->phase2.dst_addr_n < 0) s5->phase2.dst_addr_n = 0;
                if(s5->phase2.dst_addr_n > 255) s5->phase2.dst_addr_n = 255;
                continue;
            }
            if(s5->state == TYP_DOMAIN) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if(s5->phase2.dst_addr_cnt == s5->phase2.dst_addr_n) {
                    s5->state = PORT;
                    continue;
                }
            }
            if(s5->state == PORT) {
                s5->phase2.dst_port[0] = *p;
                s5->state = END;
                continue;
            }
            if(s5->state == END) {
                s5->phase2.dst_port[1] = *p;
    
                timer_del(&ev->timer);
                meta_clr(meta);  
                s5->state = 0;

                do {
                    schk(0x05 == s5->phase2.ver, break);
                    /// only support CONNECT 0x01 request
                    schk(0x01 == s5->phase2.cmd, break);
                    /// not support IPV6 request
                    schk((s5->phase2.atyp == S5_RFC_IPV4) || (s5->phase2.atyp == S5_RFC_DOMAIN), break);

                    ev->write_pt = NULL;
                    ev->read_pt = s5_srv_remote_init;
                    return ev->read_pt(ev);
                } while(0);
                s5_free(s5);
                return -1;
            }
        }
    }
}

static int s5_srv_ph1_ack(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    meta_t * meta = down->meta;

    while(meta_getlen(meta) > 0) {
        int sendn = down->send(down, meta->pos, meta_getlen(meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("s5 server rfc phase1 resp send failed\n");
                s5_free(s5);
                return -1;
            }
            if(ev->opt != EV_W) {
                event_opt(ev, down->fd, EV_W);
            }
            timer_set_data(&ev->timer, down);
            timer_set_pt(&ev->timer, s5_timeout_cb);
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        meta->pos += sendn;
    }
    timer_del(&ev->timer);
    meta_clr(meta);
    
    ///goto recv phase2 request
    ev->read_pt	= s5_srv_ph2_req;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

static int s5_srv_ph1_req(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    unsigned char * p = NULL;
    meta_t * meta = down->meta;
    /*
        s5 phase1 message req format
        1 byte	1 byte	    nmethods
        VERSION | METHODS | METHOD
    */
    enum {
        VERSION = 0,
        METHODN,
        METHOD
    };

    for(;;) {
        if(meta_getlen(meta) < 1) { ///try recv
            int recvn = down->recv(down, meta->last, meta_getfree(meta));
            if(recvn < 0) {
                if(recvn == -1) {
                    err("s5 server rfc phase1 recv failed\n");
                    s5_free(s5);
                    return recvn;
                }
                timer_set_data(&ev->timer, (void*)s5);
                timer_set_pt(&ev->timer, s5_timeout_cb);
                timer_add(&ev->timer, S5_TIMEOUT);
                return -11;
            }
            meta->last += recvn;
        }

        for(; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if(s5->state == VERSION) {
                s5->phase1.ver = *p;
                s5->state = METHODN;
                continue;
            }
            if(s5->state == METHODN) {
                s5->phase1.methods_n = *p;
                s5->phase1.methods_cnt = 0;
                s5->state = METHOD;
                continue;
            }
            if(s5->state == METHOD) {
                s5->phase1.methods[s5->phase1.methods_cnt++] = *p;
                if(s5->phase1.methods_n == s5->phase1.methods_cnt) {

                    timer_del(&ev->timer);
                    s5->state = 0;  /// reset the state 
                    meta_clr(meta);                    
                    s5_rfc_phase1_resp_t * ack = (s5_rfc_phase1_resp_t*)meta->pos;
                    ack->ver = 0x05;
                    ack->method = 0x00;
                    meta->last += sizeof(s5_rfc_phase1_resp_t);
                    /// goto send phase1 response
                    ev->read_pt = NULL;
                    ev->write_pt = s5_srv_ph1_ack;
                    return ev->write_pt(ev);
                }
            }
        }
    }
}

static int s5_srv_auth_chk(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = down->data;
    meta_t * meta = down->meta;
    
    while(meta_getlen(meta) < sizeof(s5_auth_t)) {
        int recvn = down->recv(down, meta->last, meta_getfree(meta));
        if(recvn < 0) {
            if(recvn == -11) {
                timer_set_data(&ev->timer, s5);
                timer_set_pt(&ev->timer, s5_timeout_cb);
                timer_add(&ev->timer, S5_TIMEOUT);
                return -11;
            }
            err("s5 server auth recv failed\n");
            s5_free(s5);
            return -1;
        }
        meta->last += recvn;
    }
    timer_del(&ev->timer);

    s5_auth_t * auth = (s5_auth_t*)meta->pos;
    schk(auth->magic == htonl(S5_AUTH_MAGIC), {s5_free(s5);return -1;});
    schk(ezac_find(g_s5_ctx->ac, auth->key, strlen(auth->key)) == 0, {s5_free(s5);return -1;});
    meta->pos = meta->start;
    meta->last -= sizeof(s5_auth_t);

    ev->write_pt = NULL;
    ev->read_pt = s5_srv_ph1_req;
    event_opt(ev, down->fd, EV_R);
    return ev->read_pt(ev);
}

static int s5_srv_ctx_start(event_t * ev)
{
    con_t * down = ev->data;
    if(!down->meta) {
        schk(meta_alloc(&down->meta, 8192) == 0, {net_free(down); return -1;});
    }
    s5_session_t * s5 = NULL;
    schk(s5_alloc(&s5) == 0, {net_free(down); return -1;});
    
    s5->down = down;
    down->data = s5;
    ev->read_pt	= s5_srv_auth_chk;
    return ev->read_pt(ev);
}

int s5_srv_transport(event_t * ev)
{
    con_t * down = ev->data;
    s5_session_t * s5 = NULL;

    schk(s5_alloc(&s5) == 0, {net_free(down); return -1;});
    s5->down = down;
    down->data = s5;

    ev->read_pt = s5_srv_auth_chk;
    return ev->read_pt(ev);
}

static int s5_srv_accept_chk(event_t * ev)
{
    con_t * down = ev->data;

    schk(down->ssl->f_handshaked, {net_free(down); return -1;});
    timer_del(&ev->timer);

    down->recv = ssl_read;
    down->send = ssl_write;
    down->send_chain = ssl_write_chain;

    ev->read_pt = s5_srv_ctx_start;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

int s5_srv_accept(event_t * ev)
{
    con_t * down = ev->data;
    int rc = net_check_ssl_valid(down);
    if(rc != 0) {
        if(rc == -11) {
            timer_set_data(&ev->timer, down);
            timer_set_pt(&ev->timer, net_timeout);
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        err("s5 server ssl check failed\n");
        net_free(down);
        return -1;
    }
    schk(ssl_create_connection(down, L_SSL_SERVER) == 0, {net_free(down); return -1;});
    
    rc = ssl_handshake(down->ssl);
    if(rc < 0) {
        if(rc == -11) {
            down->ssl->cb = s5_srv_accept_chk; ///!!!set cb is important
            timer_set_data(&ev->timer, down);
            timer_set_pt(&ev->timer, net_timeout);
            timer_add(&ev->timer, S5_TIMEOUT);
            return -11;
        }
        err("s5 server ssl shandshake failed\n");
        net_free(down);
        return -1;
    }
    return s5_srv_accept_chk(ev); 
}

static int s5_srv_auth_fparse(meta_t * meta)
{
    cJSON * root = cJSON_Parse((char*)meta->pos);
    if(root) {  /// traversal the array 
        int i = 0;
        for(i = 0; i < cJSON_GetArraySize(root); i++) {
            cJSON * arrobj = cJSON_GetArrayItem(root, i);
            if(0 != ezac_add(g_s5_ctx->ac, cJSON_GetStringValue(arrobj), strlen(cJSON_GetStringValue(arrobj)))) {
                err("s5 srv auth add ac err\n", cJSON_GetStringValue(arrobj));
            }
        }
        cJSON_Delete(root);
    }
    return 0;
}

static int s5_srv_auth_rfile(meta_t * meta)
{
    ssize_t size = 0;
    int fd = open((char*)config_get()->s5_serv_auth_path, O_RDONLY);
    schk(fd > 0, return -1);
    size = read(fd, meta->pos, meta_getfree(meta));
    close(fd);
    schk(size != -1, return -1);
    meta->last += size;
    return 0;
}

static int s5_srv_auth_init()
{
    meta_t * meta = NULL;
    int rc = -1;
    do {
        ;
        schk(g_s5_ctx->ac = ezac_init(), break);
        schk(meta_alloc(&meta, S5_USER_AUTH_FILE_LEN) == 0, break);
        schk(s5_srv_auth_rfile(meta) == 0, break);
        schk(s5_srv_auth_fparse(meta) == 0, break);
        ezac_compiler(g_s5_ctx->ac);
        rc = 0;
    } while(0);    
    if(meta)
        meta_free(meta);
    return rc;
}

int socks5_server_init(void)
{
    int ret = -1;
    schk(!g_s5_ctx, return -1);
    do {
        schk(g_s5_ctx = (g_s5_t*)mem_pool_alloc(sizeof(g_s5_t)), return -1);
        if(config_get()->s5_mode > SOCKS5_CLIENT) {
            schk(s5_srv_auth_init() == 0, break);
        } else {
            /// cli do nothing.
        }
        ret = 0;
    } while(0);
    if(ret == -1) {
        if(g_s5_ctx) {
            if(config_get()->s5_mode > SOCKS5_CLIENT) {
                if(g_s5_ctx->ac) ezac_free(g_s5_ctx->ac);
            }
            mem_pool_free(g_s5_ctx);
            g_s5_ctx = NULL;
        }
    }
    return 0;
}

int socks5_server_end(void)
{
    if(g_s5_ctx) {
        if(g_s5_ctx->ac) ezac_free(g_s5_ctx->ac);
        mem_pool_free((void*)g_s5_ctx);
        g_s5_ctx = NULL;
    }
    return 0;
}


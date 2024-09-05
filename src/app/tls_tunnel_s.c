#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"


#define TLS_TUNNEL_AUTH_FILE_MAX  (4*1024)

typedef struct {
    ezac_ctx_t * ac;    
} tls_tunnel_s_t;
static tls_tunnel_s_t * g_ses_ctx = NULL;

static int tls_tunnel_s_try_read(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    schk(0 == net_socket_check_status(cdown->fd), {tls_ses_free(ses); return -1;});
    return 0;
}


int tls_ses_alloc(tls_tunnel_session_t ** ses)
{
    tls_tunnel_session_t * nses = NULL;
    schk(nses = mem_pool_alloc(sizeof(tls_tunnel_session_t)), return -1);
    *ses = nses;
    return 0;
}

int tls_ses_free(tls_tunnel_session_t * ses)
{
    if(ses->cdown)
        net_free(ses->cdown);
    
    if(ses->cup)
        net_free(ses->cup);

    if(ses->dns_ses)
        dns_over(ses->dns_ses);
    
    mem_pool_free(ses);
    return 0;
}

void tls_session_timeout(void * data)
{
    tls_ses_free((tls_tunnel_session_t *)data);
}

static int tls_tunnel_traffic_recv(event_t * ev)
{
    con_t * c = ev->data;
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;    
    int recvn = 0;

    timer_set_data(&ev->timer, (void*)ses);
    timer_set_pt(&ev->timer, tls_session_timeout);
    timer_add(&ev->timer, TLS_TUNNEL_TMOUT);

    while(meta_getfree(cdown->meta) > 0) {
        recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
        if(recvn < 0) {
            if(recvn == -1) {
                err("TLS tunnel. cdown recv err\n");
                ses->frecv_err_down = 1;
            }
            break; ///break when -11 (EAGAIN)
        }
        cdown->meta->last += recvn;
    }

    if(meta_getlen(cdown->meta) > 0) {
        event_opt(cdown->event, cdown->fd, cdown->event->opt & (~EV_R));
        event_opt(cup->event, cup->fd, cup->event->opt | EV_W);
        return cup->event->write_pt(cup->event);
    }
    if(ses->frecv_err_down) {
        tls_ses_free(ses);
        return -1;
    }
    return -11;
}

static int tls_tunnel_traffic_send(event_t * ev)
{	
    con_t * c = ev->data;
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int sendn = 0;

    timer_set_data(&ev->timer, (void*)ses);
    timer_set_pt(&ev->timer, tls_session_timeout);
    timer_add(&ev->timer, TLS_TUNNEL_TMOUT);

    while(meta_getlen(cdown->meta) > 0) {
        sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("TLS tunnel cup send err\n");
                tls_ses_free(ses);
                return -1;
            }
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        cdown->meta->pos += sendn;
    }

    if(ses->frecv_err_down) {
        err("TLS tunnel. forward fin (cdown already err)\n");
        tls_ses_free(ses);
        return -1;
    }
    meta_clr(cdown->meta);
    event_opt(cup->event, cup->fd, cup->event->opt & (~EV_W));
    event_opt(cdown->event, cdown->fd, cdown->event->opt | EV_R);
    return cdown->event->read_pt(cdown->event);
}

static int tls_tunnel_traffic_reverse_recv(event_t * ev)
{
    con_t * c = ev->data;
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int recvn = 0;

    timer_set_data(&ev->timer, (void*)ses);
    timer_set_pt(&ev->timer, tls_session_timeout);
    timer_add(&ev->timer, TLS_TUNNEL_TMOUT);

    while(meta_getfree(cup->meta) > 0) {
        recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
        if(recvn < 0) {
            if(recvn == -1) {   
                err("TLS tunnel. cup recv err\n");
                ses->frecv_err_up = 1;
            }
            break;
        }
        cup->meta->last += recvn;
    }

    if(meta_getlen(cup->meta) > 0) {
        event_opt(cup->event, cup->fd, cup->event->opt & (~EV_R));
        event_opt(cdown->event, cdown->fd, cdown->event->opt | EV_W);
        return cdown->event->write_pt(cdown->event);
    }
    if(ses->frecv_err_up) {
        tls_ses_free(ses);
        return -1;
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_send(event_t * ev)
{
    con_t * c = ev->data;
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int sendn = 0;
    
    timer_set_data(&ev->timer, (void*)ses);
    timer_set_pt(&ev->timer, tls_session_timeout);
    timer_add(&ev->timer, TLS_TUNNEL_TMOUT);

    while(meta_getlen(cup->meta) > 0) {
        sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("TLS tunnel. cdown send err\n");
                tls_ses_free(ses);
                return -1;
            }
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        cup->meta->pos += sendn;
    }
    if(ses->frecv_err_up) {
        err("TLS tunnel. forward fin (cup already err)\n");
        tls_ses_free(ses);
        return -1;
    }
    meta_clr(cup->meta);
    event_opt(cdown->event, cdown->fd, cdown->event->opt & (~EV_W));
    event_opt(cup->event, cup->fd, cup->event->opt | EV_R);
    return cup->event->read_pt(cup->event);
}

int tls_tunnel_traffic_proc(event_t * ev)
{
    con_t * c = ev->data;
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;

    if(!cdown->meta) {
        schk(meta_alloc(&cdown->meta, TLS_TUNNEL_METAN) == 0, {tls_ses_free(ses);return -1;});
    }
    if(!cup->meta) {
        schk(meta_alloc(&cup->meta, TLS_TUNNEL_METAN) == 0, {tls_ses_free(ses);return -1;});
    }
    
    ///only clear up meta in here. because local run in here too.
    ///local(down) mabey recv some data.
    meta_clr(cup->meta);

    ///down -> up
    ses->cdown->event->read_pt = tls_tunnel_traffic_recv;
    ses->cup->event->write_pt = tls_tunnel_traffic_send;

    ///up -> down
    ses->cup->event->read_pt = tls_tunnel_traffic_reverse_recv;
    ses->cdown->event->write_pt = tls_tunnel_traffic_reverse_send;
    
    event_opt(ses->cup->event, ses->cup->fd, EV_R);	
    event_opt(ses->cdown->event, ses->cdown->fd, EV_R);
    return ses->cdown->event->read_pt(ses->cdown->event);
}

static int tls_tunnel_s_s5p2_rsp(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    int rc = 0;
    meta_t * meta = cdown->meta;

    while(meta_getlen(meta) > 0) {
        rc = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if(rc < 0) {
            if(rc == -1) {
                err("TLS tunnel. s5p2 rsp send failed\n");
                tls_ses_free(ses);
                return -1;
            }
            timer_set_data(&ev->timer, cdown);
            timer_set_pt(&ev->timer, tls_session_timeout);
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        meta->pos += rc;
    }
    timer_del(&ev->timer);
    meta_clr(meta);
    
    ev->read_pt = tls_tunnel_traffic_proc;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

static int tls_tunnel_s_cup_connect_chk(event_t * ev)
{
    con_t * cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;
    con_t * cdown = ses->cdown;
    meta_t * meta = cdown->meta;

    schk(net_socket_check_status(cup->fd) == 0, {tls_ses_free(ses);return -1;});
    net_socket_nodelay(cup->fd);
    timer_del(&ev->timer);

    s5_ph2_rsp_t * resp = (s5_ph2_rsp_t*)meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)cup->addr.sin_addr.s_addr);
    resp->bnd_port = htons(cup->addr.sin_port);
    meta->last += sizeof(s5_ph2_rsp_t);

    event_opt(cup->event, cup->fd, EV_NONE);
    event_opt(cdown->event, cdown->fd, EV_W);    
    cdown->event->write_pt = tls_tunnel_s_s5p2_rsp;
    return cdown->event->write_pt(cdown->event);
}

static int tls_tunnel_s_cup_connect(event_t * ev)
{
    con_t * cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;
    status rc = 0;

    rc = net_connect(ses->cup, &ses->cup->addr);
    schk(rc != -1, {tls_ses_free(ses);return -1;});
    ev->read_pt = NULL;
    ev->write_pt = tls_tunnel_s_cup_connect_chk;
    event_opt(ev, cup->fd, EV_W);
    if(rc == -11) {
        timer_set_data(&ev->timer, ses);
        timer_set_pt(&ev->timer, tls_session_timeout);
        timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
        return -11;
    }
    return ev->write_pt(ev);
}

static void tls_tunnel_s_cup_addr_cb(void * data)
{
    tls_tunnel_session_t * ses = data;
    s5_ph2_req_t * s5p2 = &ses->s5p2;
    dns_cycle_t * dns_ses = ses->dns_ses;
    char ipstr[128] = {0};

    if(dns_ses) {
        if(0 == dns_ses->dns_status) {
            uint16_t addr_port = 0;
            memcpy(&addr_port, s5p2->dst_port, sizeof(uint16_t)); 
        
            snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                dns_ses->answer.answer_addr[0],
                dns_ses->answer.answer_addr[1],
                dns_ses->answer.answer_addr[2],
                dns_ses->answer.answer_addr[3]);
            
            ses->cup->addr.sin_family = AF_INET;
            ses->cup->addr.sin_port = addr_port;
            ses->cup->addr.sin_addr.s_addr = inet_addr(ipstr);
            
            ses->cup->event->read_pt = NULL;
            ses->cup->event->write_pt = tls_tunnel_s_cup_connect;
            ses->cup->event->write_pt(ses->cup->event);
        } else {
            err("TLS tunnel. dns resolv failed\n");
            tls_ses_free(ses);
        }
    }
}

static int tls_tunnel_s_cup_addr(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_ph2_req_t * s5p2 = &ses->s5p2;
    char ipstr[128] = {0};
    status rc = 0;

    cdown->event->read_pt = tls_tunnel_s_try_read;
    cdown->event->write_pt = NULL;
    event_opt(cdown->event, cdown->fd, EV_R);

    schk(net_alloc(&ses->cup) == 0, {tls_ses_free(ses);return -1;});
    ses->cup->data = ses;

    if(s5p2->atyp == S5_RFC_IPV4) {
        uint16_t addr_port = 0;
        memcpy(&addr_port, s5p2->dst_port, sizeof(uint16_t));
        snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
            (unsigned char )s5p2->dst_addr[0],
            (unsigned char )s5p2->dst_addr[1],
            (unsigned char )s5p2->dst_addr[2],
            (unsigned char )s5p2->dst_addr[3]);
        
        ses->cup->addr.sin_family = AF_INET;
        ses->cup->addr.sin_port = addr_port;
        ses->cup->addr.sin_addr.s_addr = inet_addr(ipstr);

        ses->cup->event->read_pt = NULL;
        ses->cup->event->write_pt = tls_tunnel_s_cup_connect;
        return ses->cup->event->write_pt(ses->cup->event);
        
    }  else if (s5p2->atyp == S5_RFC_DOMAIN) {
        if(0 == dns_rec_find((char*)s5p2->dst_addr, ipstr)) {
            uint16_t addr_port = 0;
            memcpy(&addr_port, s5p2->dst_port, sizeof(uint16_t));
            snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                (unsigned char )ipstr[0],
                (unsigned char )ipstr[1],
                (unsigned char )ipstr[2],
                (unsigned char )ipstr[3]);
            ses->cup->addr.sin_family = AF_INET;
            ses->cup->addr.sin_port = addr_port;
            ses->cup->addr.sin_addr.s_addr = inet_addr(ipstr);

            ses->cup->event->read_pt = NULL;
            ses->cup->event->write_pt = tls_tunnel_s_cup_connect;
            return ses->cup->event->write_pt(ses->cup->event);
            
        } else { 
            rc = dns_create(&ses->dns_ses);
            schk(rc != -1, {tls_ses_free(ses);return -1;});
            strncpy((char*)ses->dns_ses->query, (char*)s5p2->dst_addr, sizeof(ses->dns_ses->query));
            ses->dns_ses->cb = tls_tunnel_s_cup_addr_cb;
            ses->dns_ses->cb_data = ses;
            return dns_start(ses->dns_ses);
        }
    }
    err("TLS tunnel. s5p2 atyp [0x%x]. not support\n", s5p2->atyp);
    tls_ses_free(ses);
    return -1;
}

static int tls_tunnel_s_s5p2_req(event_t * ev)
{
    unsigned char * p = NULL;
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_ph2_req_t * s5p2 = &ses->s5p2;
    meta_t * meta = cdown->meta;
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
        if(meta_getlen(meta) < 1) {
            int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
            if(recvn < 0) {
                if(recvn == -1) {
                    err("TLS tunnel. s5p2 recv failed\n");
                    tls_ses_free(ses);
                    return -1;
                }
                timer_set_data(&ev->timer, (void*)ses);
                timer_set_pt(&ev->timer, tls_session_timeout);
                timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
                return -11;
            }
            meta->last += recvn;
        }
        
        for(; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if(ses->s5_state == VER) {  ///ver is fixed. 0x05
                s5p2->ver = *p;
                ses->s5_state = CMD;
                continue;
            }
            if(ses->s5_state == CMD) {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5p2->cmd = *p;
                ses->s5_state = RSV;
                continue;
            }
            if(ses->s5_state == RSV) {    // RSV means resverd
                s5p2->rsv = *p;
                ses->s5_state = TYP;
                continue;
            }
            if(ses->s5_state == TYP) {
                s5p2->atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet of domain part
                    0x04		ipv6		16
                */
                if(s5p2->atyp == S5_RFC_IPV4) {
                    ses->s5_state = TYP_V4;
                    s5p2->dst_addr_n = 4;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                } else if (s5p2->atyp == S5_RFC_IPV6) {
                    ses->s5_state = TYP_V6;
                    s5p2->dst_addr_n = 16;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                } else if (s5p2->atyp == S5_RFC_DOMAIN) {
                    /// atpy domain -> dst addr domain len -> dst addr domain
                    ses->s5_state = TYP_DOMAINN;
                    s5p2->dst_addr_n = 0;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                }
                err("TLS tunnel. s5p2 atyp [%d] not support\n", s5p2->atyp);
                tls_ses_free(ses);
                return -1;
            }
            if(ses->s5_state == TYP_V4) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == 4) {
                    ses->s5_state = PORT;
                    continue;
                }
            }
            if(ses->s5_state == TYP_V6) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == 16) {
                    ses->s5_state = PORT;
                    continue;
                }
            }
            if(ses->s5_state == TYP_DOMAINN) {
                s5p2->dst_addr_n = *p;
                ses->s5_state = TYP_DOMAIN;
                if(s5p2->dst_addr_n < 0) s5p2->dst_addr_n = 0;
                if(s5p2->dst_addr_n > 255) s5p2->dst_addr_n = 255;
                continue;
            }
            if(ses->s5_state == TYP_DOMAIN) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == s5p2->dst_addr_n) {
                    ses->s5_state = PORT;
                    continue;
                }
            }
            if(ses->s5_state == PORT) {
                s5p2->dst_port[0] = *p;
                ses->s5_state = END;
                continue;
            }
            if(ses->s5_state == END) {
                s5p2->dst_port[1] = *p;
    
                timer_del(&ev->timer);
                ses->s5_state = 0;

                do {
                    schk(0x05 == s5p2->ver, break);                    
                    schk(0x01 == s5p2->cmd, break);    /// only support CONNECT 0x01 request
                    schk(s5p2->atyp != S5_RFC_IPV6, break); /// not support IPV6 request

                    meta_clr(meta);
                    ev->write_pt = NULL;
                    ev->read_pt = tls_tunnel_s_cup_addr;
                    return ev->read_pt(ev);
                } while(0);
                tls_ses_free(ses);
                return -1;
            }
        }
    }
}

static int tls_tunnel_s_s5p1_rsp(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    meta_t * meta = cdown->meta;

    while(meta_getlen(meta) > 0) {
        int sendn = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("TLS tunnel. s5p1 rsp send failed\n");
                tls_ses_free(ses);
                return -1;
            }
            if(ev->opt != EV_W)
                event_opt(ev, cdown->fd, EV_W);
            
            timer_set_data(&ev->timer, cdown);
            timer_set_pt(&ev->timer, tls_session_timeout);
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        meta->pos += sendn;
    }
    timer_del(&ev->timer);
    
    meta_clr(meta);
    ev->read_pt	= tls_tunnel_s_s5p2_req;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

static int tls_tunnel_s_s5p1_req(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_ph1_req_t * s5p1 = &ses->s5p1;
    unsigned char * p = NULL;
    meta_t * meta = cdown->meta;
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
            int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
            if(recvn < 0) {
                if(recvn == -1) {
                    err("TLS tunnel. s5p1 recv failed\n");
                    tls_ses_free(ses);
                    return recvn;
                }
                timer_set_data(&ev->timer, (void*)ses);
                timer_set_pt(&ev->timer, tls_session_timeout);
                timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
                return -11;
            }
            meta->last += recvn;
        }

        for(; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if(ses->s5_state == VERSION) {
                s5p1->ver = *p;
                ses->s5_state = METHODN;
                continue;
            }
            if(ses->s5_state == METHODN) {
                s5p1->methods_n = *p;
                s5p1->methods_cnt = 0;
                ses->s5_state = METHOD;
                continue;
            }
            if(ses->s5_state == METHOD) {
                s5p1->methods[s5p1->methods_cnt++] = *p;
                if(s5p1->methods_n == s5p1->methods_cnt) {

                    timer_del(&ev->timer);
                    ses->s5_state = 0;
                    
                    meta_clr(meta);
                    s5_ph1_rsp_t * ack = (s5_ph1_rsp_t*)meta->pos;
                    ack->ver = 0x05;
                    ack->method = 0x00;
                    meta->last += sizeof(s5_ph1_rsp_t);
                    
                    ev->read_pt = NULL;
                    ev->write_pt = tls_tunnel_s_s5p1_rsp;
                    return ev->write_pt(ev);
                }
            }
        }
    }
}

static int tls_tunnel_s_auth_chk(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    meta_t * meta = cdown->meta;
    
    while(meta_getlen(meta) < sizeof(tls_tunnel_auth_t)) {
        int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
        if(recvn < 0) {
            if(recvn == -11) {
                timer_set_data(&ev->timer, ses);
                timer_set_pt(&ev->timer, tls_session_timeout);
                timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel auth recv failed\n");
            tls_ses_free(ses);
            return -1;
        }
        meta->last += recvn;
    }
    timer_del(&ev->timer);

    tls_tunnel_auth_t * auth = (tls_tunnel_auth_t*)meta->pos;
    int auth_chk = -1;
    do {
        schk(auth->magic == htonl(TLS_TUNNEL_AUTH_MAGIC_NUM), break);
        schk(0 == ezac_find(g_ses_ctx->ac, auth->key, strlen(auth->key)), break);
        auth_chk = 0;
    } while(0);
    
    if(0 != auth_chk) {
       tls_ses_free(ses); 
       return -1;
    }
    meta_clr(meta);
    ev->write_pt = NULL;
    ev->read_pt = tls_tunnel_s_s5p1_req;
    event_opt(ev, cdown->fd, EV_R);
    return ev->read_pt(ev);
}

static int tls_tunnel_s_start(event_t * ev)
{
    con_t * cdown = ev->data;
    if(!cdown->meta)
        schk(0 == meta_alloc(&cdown->meta, TLS_TUNNEL_METAN), {net_free(cdown); return -1;});

    tls_tunnel_session_t * ses = NULL;
    schk(0 == tls_ses_alloc(&ses), {net_free(cdown); return -1;});
    
    ses->cdown = cdown;
    cdown->data = ses;
    ev->read_pt	= tls_tunnel_s_auth_chk;
    return ev->read_pt(ev);
}

int tls_tunnel_s_transport(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = NULL;

    schk(tls_ses_alloc(&ses) == 0, {net_free(cdown); return -1;});
    ses->cdown = cdown;
    cdown->data = ses;

    ev->read_pt = tls_tunnel_s_auth_chk;
    return ev->read_pt(ev);
}

static int tls_tunnel_s_accept_chk(event_t * ev)
{
    con_t * cdown = ev->data;

    if(!cdown->ssl->f_handshaked) {
        err("TLS tunnel. handshake err\n");
        net_free(cdown);
        return -1;
    }
    timer_del(&ev->timer);

    cdown->recv = ssl_read;
    cdown->send = ssl_write;
    cdown->send_chain = ssl_write_chain;

    ev->read_pt = tls_tunnel_s_start;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}

int tls_tunnel_s_accept(event_t * ev)
{
    con_t * cdown = ev->data;
    int rc = net_check_ssl_valid(cdown);
    if(rc != 0) {
        if(rc == -11) {
            timer_set_data(&ev->timer, cdown);
            timer_set_pt(&ev->timer, net_timeout);
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel ssl chk failed\n");
        net_free(cdown);
        return -1;
    }
    schk(ssl_create_connection(cdown, L_SSL_SERVER) == 0, {net_free(cdown); return -1;});
    
    rc = ssl_handshake(cdown->ssl);
    if(rc < 0) {
        if(rc == -11) {
            cdown->ssl->cb = tls_tunnel_s_accept_chk; ///!!!set cb is important
            timer_set_data(&ev->timer, cdown);
            timer_set_pt(&ev->timer, net_timeout);
            timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel. shandshake failed\n");
        net_free(cdown);
        return -1;
    }
    return tls_tunnel_s_accept_chk(ev); 
}

static int tls_tunnel_s_auth_fparse(meta_t * meta)
{
    cJSON * root = cJSON_Parse((char*)meta->pos);
    if(root) {  /// traversal the array 
        int i = 0;
        for(i = 0; i < cJSON_GetArraySize(root); i++) {
            cJSON * arrobj = cJSON_GetArrayItem(root, i);
            if(0 != ezac_add(g_ses_ctx->ac, cJSON_GetStringValue(arrobj), strlen(cJSON_GetStringValue(arrobj)))) {
                err("s5 srv auth add ac err\n", cJSON_GetStringValue(arrobj));
            }
        }
        cJSON_Delete(root);
    }
    return 0;
}

static int tls_tunnel_s_auth_fread(meta_t * meta)
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

static int tls_tunnel_s_auth_init()
{
    meta_t * meta = NULL;
    int rc = -1;
    do {
        schk(g_ses_ctx->ac = ezac_init(), break);
        schk(meta_alloc(&meta, TLS_TUNNEL_AUTH_FILE_MAX) == 0, break);
        schk(tls_tunnel_s_auth_fread(meta) == 0, break);
        schk(tls_tunnel_s_auth_fparse(meta) == 0, break);
        ezac_compiler(g_ses_ctx->ac);
        rc = 0;
    } while(0);    
    if(meta)
        meta_free(meta);
    return rc;
}

int tls_tunnel_s_init(void)
{
    schk(!g_ses_ctx, return -1);
    schk(g_ses_ctx = (tls_tunnel_s_t*)mem_pool_alloc(sizeof(tls_tunnel_s_t)), return -1);
    if(config_get()->s5_mode > TLS_TUNNEL_C) 
        schk(tls_tunnel_s_auth_init() == 0, return -1);
    return 0;
}

int tls_tunnel_s_exit(void)
{
    if(g_ses_ctx) {
        if(g_ses_ctx->ac) 
            ezac_free(g_ses_ctx->ac);
        
        mem_pool_free((void*)g_ses_ctx);
        g_ses_ctx = NULL;
    }
    return 0;
}


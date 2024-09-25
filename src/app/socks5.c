#include "common.h"
#include "dns.h"
#include "tls_tunnel_s.h"
#include "socks5.h"

static int s5_try_read(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    schk(0 == net_socket_check_status(cdown->fd), {tls_ses_free(ses); return -1;});
    return 0;
}

int s5_p2_rsp(event_t * ev)
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

int s5_cup_connect_chk(event_t * ev)
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
    cdown->event->write_pt = s5_p2_rsp;
    return cdown->event->write_pt(cdown->event);
}

int s5_cup_connect(event_t * ev)
{
    con_t * cup = ev->data;
    tls_tunnel_session_t * ses = cup->data;
    status rc = 0;

    rc = net_connect(ses->cup, &ses->cup->addr);
    schk(rc != -1, {tls_ses_free(ses);return -1;});
    ev->read_pt = NULL;
    ev->write_pt = s5_cup_connect_chk;
    event_opt(ev, cup->fd, EV_W);
    if(rc == -11) {
        timer_set_data(&ev->timer, ses);
        timer_set_pt(&ev->timer, tls_session_timeout);
        timer_add(&ev->timer, TLS_TUNNEL_TMOUT);
        return -11;
    }
    return ev->write_pt(ev);
}

void s5_cup_addr_cb(void * data)
{
    tls_tunnel_session_t * ses = data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph2_req_t * s5p2 = &s5->s5p2;
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
            ses->cup->event->write_pt = s5_cup_connect;
            ses->cup->event->write_pt(ses->cup->event);
        } else {
            err("TLS tunnel. dns resolv failed\n");
            tls_ses_free(ses);
        }
    }
}

int s5_cup_addr(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph2_req_t * s5p2 = &s5->s5p2;
    char ipstr[128] = {0};
    status rc = 0;

    cdown->event->read_pt = s5_try_read;
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
        ses->cup->event->write_pt = s5_cup_connect;
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
            ses->cup->event->write_pt = s5_cup_connect;
            return ses->cup->event->write_pt(ses->cup->event);
            
        } else { 
            rc = dns_create(&ses->dns_ses);
            schk(rc != -1, {tls_ses_free(ses);return -1;});
            strncpy((char*)ses->dns_ses->query, (char*)s5p2->dst_addr, sizeof(ses->dns_ses->query));
            ses->dns_ses->cb = s5_cup_addr_cb;
            ses->dns_ses->cb_data = ses;
            return dns_start(ses->dns_ses);
        }
    }
    err("s5p2 atyp [0x%x]. not support\n", s5p2->atyp);
    tls_ses_free(ses);
    return -1;
}


int s5_p2_req(event_t * ev)
{
    unsigned char * p = NULL;
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph2_req_t * s5p2 = &s5->s5p2;
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
                    err("s5p2 recv failed\n");
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
            if(s5->s5_state == VER) {  ///ver is fixed. 0x05
                s5p2->ver = *p;
                s5->s5_state = CMD;
                continue;
            }
            if(s5->s5_state == CMD) {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5p2->cmd = *p;
                s5->s5_state = RSV;
                continue;
            }
            if(s5->s5_state == RSV) {    // RSV means resverd
                s5p2->rsv = *p;
                s5->s5_state = TYP;
                continue;
            }
            if(s5->s5_state == TYP) {
                s5p2->atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet of domain part
                    0x04		ipv6		16
                */
                if(s5p2->atyp == S5_RFC_IPV4) {
                    s5->s5_state = TYP_V4;
                    s5p2->dst_addr_n = 4;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                } else if (s5p2->atyp == S5_RFC_IPV6) {
                    s5->s5_state = TYP_V6;
                    s5p2->dst_addr_n = 16;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                } else if (s5p2->atyp == S5_RFC_DOMAIN) {
                    /// atpy domain -> dst addr domain len -> dst addr domain
                    s5->s5_state = TYP_DOMAINN;
                    s5p2->dst_addr_n = 0;
                    s5p2->dst_addr_cnt = 0;
                    continue;
                }
                err("s5p2 atyp [%d] not support\n", s5p2->atyp);
                tls_ses_free(ses);
                return -1;
            }
            if(s5->s5_state == TYP_V4) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == 4) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if(s5->s5_state == TYP_V6) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == 16) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if(s5->s5_state == TYP_DOMAINN) {
                s5p2->dst_addr_n = *p;
                s5->s5_state = TYP_DOMAIN;
                if(s5p2->dst_addr_n < 0) s5p2->dst_addr_n = 0;
                if(s5p2->dst_addr_n > 255) s5p2->dst_addr_n = 255;
                continue;
            }
            if(s5->s5_state == TYP_DOMAIN) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if(s5p2->dst_addr_cnt == s5p2->dst_addr_n) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if(s5->s5_state == PORT) {
                s5p2->dst_port[0] = *p;
                s5->s5_state = END;
                continue;
            }
            if(s5->s5_state == END) {
                s5p2->dst_port[1] = *p;
    
                timer_del(&ev->timer);
                s5->s5_state = 0;

                do {
                    schk(0x05 == s5p2->ver, break);                    
                    schk(0x01 == s5p2->cmd, break);    /// only support CONNECT 0x01 request
                    schk(s5p2->atyp != S5_RFC_IPV6, break); /// not support IPV6 request

                    meta_clr(meta);
                    ev->write_pt = NULL;
                    ev->read_pt = s5_cup_addr;
                    return ev->read_pt(ev);
                } while(0);
                tls_ses_free(ses);
                return -1;
            }
        }
    }
}


int s5_p1_rsp(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    meta_t * meta = cdown->meta;

    while(meta_getlen(meta) > 0) {
        int sendn = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("s5p1 rsp send failed\n");
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
    ev->read_pt	= s5_p2_req;
    ev->write_pt = NULL;
    return ev->read_pt(ev);
}


int s5_p1_req(event_t * ev)
{
    con_t * cdown = ev->data;
    tls_tunnel_session_t * ses = cdown->data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph1_req_t * s5p1 = &s5->s5p1;
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
                    err("s5p1 recv failed\n");
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
            if(s5->s5_state == VERSION) {
                s5p1->ver = *p;
                s5->s5_state = METHODN;
                continue;
            }
            if(s5->s5_state == METHODN) {
                s5p1->methods_n = *p;
                s5p1->methods_cnt = 0;
                s5->s5_state = METHOD;
                continue;
            }
            if(s5->s5_state == METHOD) {
                s5p1->methods[s5p1->methods_cnt++] = *p;
                if(s5p1->methods_n == s5p1->methods_cnt) {

                    timer_del(&ev->timer);
                    s5->s5_state = 0;
                    
                    meta_clr(meta);
                    s5_ph1_rsp_t * ack = (s5_ph1_rsp_t*)meta->pos;
                    ack->ver = 0x05;
                    ack->method = 0x00;
                    meta->last += sizeof(s5_ph1_rsp_t);
                    
                    ev->read_pt = NULL;
                    ev->write_pt = s5_p1_rsp;
                    return ev->write_pt(ev);
                }
            }
        }
    }
}

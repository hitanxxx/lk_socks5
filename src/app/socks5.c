#include "common.h"
#include "socks5.h"
#include "dns.h"
#include "tls_tunnel_s.h"

static int s5_cdown_recv(con_t *cdown) {
    tls_tunnel_session_t *session = cdown->user_data;
    meta_t *meta = cdown->meta;

    meta_clr(meta);
    for (;;) {
        if (meta_getfree(meta) > 0) {
            int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
            if (recvn < 0) {
                if (recvn == -11) {
                    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                    return -11;
                }
                err("s5. cdown wait cup connect. recv err.\n");
                net_free(session->cup);
                net_free(cdown);
                return -1;
            }
        }
    }
    return 0;
}

int s5_p2_rsp(con_t *cdown) {
    tls_tunnel_session_t *session = cdown->user_data;
    meta_t *meta = cdown->meta;
    
    while (meta_getlen(meta) > 0) {
        int rc = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if (rc < 0) {
            if (rc == -11) {
                net_ev_set(cdown, EV_W);
                net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                return -11;
            }
            err("s5. p2 rsp send err\n");
            net_free(session->cup);
            net_free(cdown);
            return -1;
        }
        meta->pos += rc;
    }
    net_timer_del(cdown);
    meta_clr(meta);

    cdown->read_cb = tls_tunnel_traffic_proc;
    cdown->write_cb = NULL;
    return cdown->read_cb(cdown);
}

int s5_cup_connect_chk(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;
    con_t *cdown = session->cdown;
    meta_t *meta = cdown->meta;

    net_timer_del(cup);

    if (0 != net_connect_chk(cup)) {
        err("s5. cup connect chk err\n");
        net_free(cup);
        net_free(cdown);
        return -1;
    }
    
    meta_clr(meta);
    s5_ph2_rsp_t *resp = (s5_ph2_rsp_t *)meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)cup->addr.sin_addr.s_addr);
    resp->bnd_port = htons(cup->addr.sin_port);
    meta->last += sizeof(s5_ph2_rsp_t);

    cup->read_cb = NULL;
    cup->write_cb = NULL;

    cdown->read_cb = NULL;
    cdown->write_cb = s5_p2_rsp;
    return cdown->write_cb(cdown);
}

int s5_cup_connect(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;

    cup->read_cb = s5_cup_connect_chk;
    cup->write_cb = s5_cup_connect_chk;
    
    int rc = net_connect(session->cup, NULL, 1);
    if (rc < 0) {
        if (rc == -11) {
            net_timer_add(cup, tls_session_timeout, TLS_TMOUT);
            return -11;
        }
        err("s5. cup connect err\n");
        net_free(cup);
        net_free(session->cdown);
        return -1;
    }
    
    return cup->write_cb(cup);
}

void s5_cup_dns_cb(int status, uint8_t *result, void *data) {
    tls_tunnel_session_t *session = data;
    s5_t *s5 = (s5_t *)session->adata;
    s5_ph2_req_t *s5p2 = &s5->s5p2;

    ///clear dns ctx in callback function.
    ///avoid clear repeat when session release
    session->dns = NULL;

    if (status == 0) {
        session->cup->addr.sin_family = AF_INET;
        memcpy(&session->cup->addr.sin_port, s5p2->dst_port, 2);
        memcpy(&session->cup->addr.sin_addr.s_addr, result, 4);
        session->cup->write_cb(session->cup);
    } else {
        err("s5. dns cb resolv err\n");
        net_free(session->cup);
        net_free(session->cdown);
    }
    return;
}

int s5_cup_addr(con_t *cdown) {
    tls_tunnel_session_t *session = cdown->user_data;
    s5_t *s5 = (s5_t *)session->adata;
    s5_ph2_req_t *s5p2 = &s5->s5p2;
    uint8_t ipstr[128] = {0};

    cdown->read_cb = s5_cdown_recv;
    cdown->write_cb = NULL;

    schk(net_alloc(&session->cup) == 0, {
        net_free(cdown);
        return -1;
    });
    session->cup->user_data = session;
    session->cup->free_user_data = tls_session_release_by_cup;
    session->cup->read_cb = NULL;
    session->cup->write_cb = s5_cup_connect;

    ///IPV4 TYP
    if (s5p2->atyp == S5_RFC_IPV4) {
        session->cup->addr.sin_family = AF_INET;
        memcpy(&session->cup->addr.sin_port, s5p2->dst_port, 2);
        memcpy(&session->cup->addr.sin_addr.s_addr, s5p2->dst_addr, 4);
        return session->cup->write_cb(session->cup);
    } 

    /// DOMAIN TYP.
    /// 1.dns cache find out
    /// 2.dns cache not found. goto resolve
    if (0 == dns_record_find((char*)s5p2->dst_addr, ipstr)) {
        session->cup->addr.sin_family = AF_INET;
        memcpy(&session->cup->addr.sin_port, s5p2->dst_port, 2);
        memcpy(&session->cup->addr.sin_addr.s_addr, ipstr, 4);
        return session->cup->write_cb(session->cup);
    } 

    session->dns = dns_resolve((char*)s5p2->dst_addr, s5_cup_dns_cb, session);
    if (!session->dns) {
        err("s5. dns resolve err\n");
        net_free(session->cup);
        net_free(session->cdown);
        return -1;
    }
    return 0;
}

int s5_p2_req(con_t *cdown) {
    uint8_t *p = NULL;

    tls_tunnel_session_t *session = cdown->user_data;
    s5_t *s5 = (s5_t *)session->adata;
    s5_ph2_req_t *s5p2 = &s5->s5p2;
    meta_t *meta = cdown->meta;

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

    for (;;) {
        if (meta_getlen(meta) < 1) {
            int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
            if (recvn < 0) {
                if (recvn == -11) {
                    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                    return -11;
                }
                err("s5. p2 recv err\n");
                net_free(cdown);
                return -1;
            }
            meta->last += recvn;
        }

        for (; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if (s5->s5_state == VER) { /// ver is fixed. 0x05
                s5p2->ver = *p;
                s5->s5_state = CMD;
                continue;
            }
            if (s5->s5_state == CMD) {
                /*
                    socks5 support cmd value
                    01                connect
                    02                bind
                    03                udp associate
                */
                s5p2->cmd = *p;
                s5->s5_state = RSV;
                continue;
            }
            if (s5->s5_state == RSV) { // RSV means resverd
                s5p2->rsv = *p;
                s5->s5_state = TYP;
                continue;
            }
            if (s5->s5_state == TYP) {
                s5p2->atyp = *p;
                /*
                    atyp        type        length
                    0x01        ipv4        4
                    0x03        domain        first octet of domain part
                    0x04        ipv6        16
                */
                if (s5p2->atyp == S5_RFC_IPV4) {
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
                err("s5. p2 atyp [%d] not support\n", s5p2->atyp);
                net_free(cdown);
                return -1;
            }
            if (s5->s5_state == TYP_V4) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if (s5p2->dst_addr_cnt == 4) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if (s5->s5_state == TYP_V6) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if (s5p2->dst_addr_cnt == 16) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if (s5->s5_state == TYP_DOMAINN) {
                s5p2->dst_addr_n = *p;
                s5->s5_state = TYP_DOMAIN;
                if (s5p2->dst_addr_n < 0)
                    s5p2->dst_addr_n = 0;
                if (s5p2->dst_addr_n > 255)
                    s5p2->dst_addr_n = 255;
                continue;
            }
            if (s5->s5_state == TYP_DOMAIN) {
                s5p2->dst_addr[(int)s5p2->dst_addr_cnt++] = *p;
                if (s5p2->dst_addr_cnt == s5p2->dst_addr_n) {
                    s5->s5_state = PORT;
                    continue;
                }
            }
            if (s5->s5_state == PORT) {
                s5p2->dst_port[0] = *p;
                s5->s5_state = END;
                continue;
            }
            if (s5->s5_state == END) {
                s5p2->dst_port[1] = *p;

                s5->s5_state = 0;
                net_timer_del(cdown);
                meta_clr(meta);

                do {
                    schk(0x05 == s5p2->ver, break);
                    schk(0x01 == s5p2->cmd, break); /// only support CONNECT 0x01 request
                    schk(((s5p2->atyp == S5_RFC_IPV4) || (s5p2->atyp == S5_RFC_DOMAIN)), break); /// not support IPV6 request

                    cdown->read_cb = s5_cup_addr;
                    cdown->write_cb = NULL;
                    return cdown->read_cb(cdown);
                } while (0);

                net_free(cdown);
                return -1;
            }
        }
    }
}

int s5_p1_rsp(con_t *cdown) {
    meta_t *meta = cdown->meta;

    while (meta_getlen(meta) > 0) {
        int sendn = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if (sendn < 0) {
            if (sendn == -11) {
                net_ev_set(cdown, EV_W);
                net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                return -11;
            }
            err("s5. p1 rsp send err\n");
            net_free(cdown);
            return -1;
        }
        meta->pos += sendn;
    }
    net_timer_del(cdown);
    meta_clr(meta);

    net_ev_set(cdown, EV_R);
    cdown->read_cb = s5_p2_req;
    cdown->write_cb = NULL;
    return cdown->read_cb(cdown);
}

int s5_p1_req(con_t *cdown) {
    tls_tunnel_session_t *session = cdown->user_data;
    s5_t *s5 = (s5_t *)session->adata;
    s5_ph1_req_t *s5p1 = &s5->s5p1;
    unsigned char *p = NULL;
    meta_t *meta = cdown->meta;
    /*
        s5 phase1 message req format
        1 byte    1 byte        nmethods
        VERSION | METHODS | METHOD
    */
    enum { VERSION = 0, METHODN, METHOD };

    for (;;) {
        if (meta_getlen(meta) < 1) { /// try recv
            int recvn = cdown->recv(cdown, meta->last, meta_getfree(meta));
            if (recvn < 0) {
                if (recvn == -11) {
                    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                    return -11;
                }
                err("s5. p1 recv err\n");
                net_free(cdown);
                return -1;
            }
            meta->last += recvn;
        }

        for (; meta->pos < meta->last; meta->pos++) {
            p = meta->pos;
            if (s5->s5_state == VERSION) {
                s5p1->ver = *p;
                s5->s5_state = METHODN;
                continue;
            }
            if (s5->s5_state == METHODN) {
                s5p1->methods_n = *p;
                s5p1->methods_cnt = 0;
                s5->s5_state = METHOD;
                continue;
            }
            if (s5->s5_state == METHOD) {
                s5p1->methods[s5p1->methods_cnt++] = *p;
                if (s5p1->methods_n == s5p1->methods_cnt) {
                    net_timer_del(cdown);

                    s5->s5_state = 0;
                    meta_clr(meta);

                    s5_ph1_rsp_t *ack = (s5_ph1_rsp_t *)meta->pos;
                    ack->ver = 0x05;
                    ack->method = 0x00;
                    meta->last += sizeof(s5_ph1_rsp_t);

                    cdown->read_cb = NULL;
                    cdown->write_cb = s5_p1_rsp;
                    return cdown->write_cb(cdown);
                }
            }
        }
    }
}

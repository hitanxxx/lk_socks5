#include "common.h"
#include "dns.h"
#include "tls_tunnel_s.h"
#include "socks5.h"

static int s5_try_read(con_t * cdown)
{
    tls_tunnel_session_t * ses = cdown->data;

    if(0 != net_socket_check_status(cdown->fd)) {
        err("tls tunnel. socket chk err\n");
        net_free(ses->cup);
        net_free(cdown);
        return -1;
    }
    return 0;
}

int s5_p2_rsp(con_t * cdown)
{
    tls_tunnel_session_t * ses = cdown->data;
    int rc = 0;
    meta_t * meta = cdown->meta;

    while(meta_getlen(meta) > 0) {
        rc = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if(rc < 0) {
            if(rc == -11) {
                tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. s5p2 rsp send failed\n");
            net_free(ses->cup);
            net_free(cdown);
            return -1;
        }
        meta->pos += rc;
    }
    tm_del(cdown);
    meta_clr(meta);
    
    cdown->ev->read_cb = tls_tunnel_traffic_proc;
    cdown->ev->write_cb = NULL;
    return cdown->ev->read_cb(cdown);
}

int s5_cup_connect_chk(con_t * cup)
{
    tls_tunnel_session_t * ses = cup->data;
    con_t * cdown = ses->cdown;
    meta_t * meta = cdown->meta;

    tm_del(cup);

    if(0 != net_socket_check_status(cup->fd)) {
        err("tls tunnel. socket chk err\n");
        net_free(cup);
        net_free(cdown);
        return -1;
    }
    net_socket_nodelay(cup->fd);

    s5_ph2_rsp_t * resp = (s5_ph2_rsp_t*)meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)cup->addr.sin_addr.s_addr);
    resp->bnd_port = htons(cup->addr.sin_port);
    meta->last += sizeof(s5_ph2_rsp_t);

    cup->ev->read_cb = NULL;
    cup->ev->write_cb = NULL;

    cdown->ev->read_cb = NULL;
    cdown->ev->write_cb = s5_p2_rsp;
    return cdown->ev->write_cb(cdown);
}

int s5_cup_connect(con_t * cup)
{
    tls_tunnel_session_t * ses = cup->data;
    status rc = 0;

    cup->ev->read_cb = NULL;
    cup->ev->write_cb = s5_cup_connect_chk;

    rc = net_connect(ses->cup, &ses->cup->addr);
    if(rc < 0) {
        if(rc == -11) {
            tm_add(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("socks5 connect err\n");
        net_free(cup);
        net_free(ses->cdown);
        return -1;
    }
    return cup->ev->write_cb(cup);
}

void s5_cup_dns_cb(int status, unsigned char * result, void * data)
{
    tls_tunnel_session_t * ses = data;
    dnsc_t * dnsc = ses->dns;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph2_req_t * s5p2 = &s5->s5p2;
    char ipstr[128] = {0};


    if(status == 0) {
        uint16_t addr_port = 0;
        memcpy(&addr_port, s5p2->dst_port, sizeof(uint16_t));
        snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d",
            result[0], result[1], result[2], result[3]);
 
        //after result used 
        dns_free(dnsc); 	

        ses->cup->addr.sin_family = AF_INET;
        ses->cup->addr.sin_port = addr_port;
        ses->cup->addr.sin_addr.s_addr = inet_addr(ipstr);
        
        ses->cup->ev->read_cb = NULL;
        ses->cup->ev->write_cb = s5_cup_connect;
        ses->cup->ev->write_cb(ses->cup);
    } else {
        err("TLS tunnel. dns resolv failed\n");
        dns_free(dnsc);
        net_free(ses->cup);
        net_free(ses->cdown);
    }
    return;
}

int s5_cup_addr(con_t * cdown)
{
    tls_tunnel_session_t * ses = cdown->data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph2_req_t * s5p2 = &s5->s5p2;
    char ipstr[128] = {0};
    
    cdown->ev->read_cb = s5_try_read;
    cdown->ev->write_cb = NULL;

    if(s5p2->atyp == S5_RFC_IPV4 || s5p2->atyp == S5_RFC_DOMAIN) {
        schk(net_alloc(&ses->cup) == 0, {
            net_free(cdown);
            return -1;
        });
        ses->cup->data = ses;
        ses->cup->data_cb = tls_ses_release_cup;

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

            ses->cup->ev->read_cb = NULL;
            ses->cup->ev->write_cb = s5_cup_connect;
            return ses->cup->ev->write_cb(ses->cup);
        }
        ///DOMAIN typ
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

            ses->cup->ev->read_cb = NULL;
            ses->cup->ev->write_cb = s5_cup_connect;
            return ses->cup->ev->write_cb(ses->cup);
        } else {
            return dns_alloc(&ses->dns,
                            (char*)s5p2->dst_addr,
                            s5_cup_dns_cb,
                            ses);
        }
    }

    err("s5p2 atyp [0x%x]. not support\n", s5p2->atyp);
    net_free(cdown);
    return -1;
}


int s5_p2_req(con_t * cdown)
{
    unsigned char * p = NULL;

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
                if(recvn == -11) {
                    tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
                    return -11;
                }
                err("s5p2 recv failed\n");
                net_free(cdown);
                return -1;
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
                    01                connect
                    02                bind
                    03                udp associate
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
                    atyp        type        length
                    0x01        ipv4        4
                    0x03        domain        first octet of domain part
                    0x04        ipv6        16
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
                net_free(cdown);
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

                s5->s5_state = 0;
                tm_del(cdown);
                meta_clr(meta);

                do {
                    schk(0x05 == s5p2->ver, break);
                    schk(0x01 == s5p2->cmd, break);    /// only support CONNECT 0x01 request
                    schk(s5p2->atyp != S5_RFC_IPV6, break); /// not support IPV6 request
    
                    cdown->ev->read_cb = s5_cup_addr;
                    cdown->ev->write_cb = NULL;
                    return cdown->ev->read_cb(cdown);
                } while(0);

                net_free(cdown);
                return -1;
            }
        }
    }
}

int s5_p1_rsp(con_t * cdown)
{
    meta_t * meta = cdown->meta;

    while(meta_getlen(meta) > 0) {
        int sendn = cdown->send(cdown, meta->pos, meta_getlen(meta));
        if(sendn < 0) {
            if(sendn == -11) {
                tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("s5p1 rsp send failed\n");
            net_free(cdown);
            return -1;
        }
        meta->pos += sendn;
    }
    tm_del(cdown);
    meta_clr(meta);
    
    cdown->ev->read_cb = s5_p2_req;
    cdown->ev->write_cb = NULL;
    return cdown->ev->read_cb(cdown);
}

int s5_p1_req(con_t * cdown)
{
    tls_tunnel_session_t * ses = cdown->data;
    s5_t * s5 = (s5_t*)ses->adata;
    s5_ph1_req_t * s5p1 = &s5->s5p1;
    unsigned char * p = NULL;
    meta_t * meta = cdown->meta;
    /*
        s5 phase1 message req format
        1 byte    1 byte        nmethods
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
                if(recvn == -11) {
                    tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
                    return -11;
                }
                err("s5p1 recv failed\n");
                net_free(cdown);
                return -1;
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
                    tm_del(cdown);

                    s5->s5_state = 0;
                    meta_clr(meta);
                    
                    s5_ph1_rsp_t * ack = (s5_ph1_rsp_t*)meta->pos;
                    ack->ver = 0x05;
                    ack->method = 0x00;
                    meta->last += sizeof(s5_ph1_rsp_t);
                    
                    cdown->ev->read_cb = NULL;
                    cdown->ev->write_cb = s5_p1_rsp;
                    return cdown->ev->write_cb(cdown);
                }
            }
        }
    }
}

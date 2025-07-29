#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"


static int tls_tunnel_c_recv(con_t * cdown) 
{
    /// cache read data    
    tls_tunnel_session_t * ses = cdown->data;
    meta_t * meta = cdown->meta;
    int readn = 0;

    for(;;) {
        if(meta_getfree(meta) < 1) {
            err("TLS tunnel cdown recv ccdata too much\n");
            net_free(ses->cup);
            net_free(ses->cdown);
            return -1;
        }

        /// cache read data
        readn = cdown->recv(cdown, meta->last, meta_getfree(meta));
        if(readn < 0) {
            if(readn == -11)
                return -11;

            err("TLS tunnel cdown recv err\n");
            net_free(ses->cup);
            net_free(ses->cdown);
            return -1;
        }
        meta->last += readn;
    }
}


static inline void tls_tunnel_s_addr(struct sockaddr_in * addr)
{
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config_get()->s5_local_serv_port);
    addr->sin_addr.s_addr = inet_addr(config_get()->s5_local_serv_ip);
}

static int tls_tunnel_c_auth_send(con_t * cup)
{
    tls_tunnel_session_t * ses = cup->data;
    meta_t * meta = cup->meta;

    int sendn = cup->send_chain(cup, meta);
    if(sendn < 0) {
        if(sendn == -11) {
            tm_add(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel auth req send err\n");
        net_free(cup);
        net_free(ses->cdown);
        return -1;
    }

    tm_del(cup);
    meta_clr(meta);

    ses->cdown->ev->read_cb = tls_tunnel_traffic_proc;
    ses->cdown->ev->write_cb = NULL;
    return ses->cdown->ev->read_cb(ses->cdown);
}

static int tls_tunnel_c_auth_build(con_t * cup)
{
    meta_t * meta = cup->meta;

    net_socket_nodelay(cup->fd);

    ///build auth req
    meta_clr(meta);
    tls_tunnel_auth_t * auth = (tls_tunnel_auth_t*)meta->last;
    auth->magic = htonl(TLS_TUNNEL_AUTH_MAGIC_NUM);
    memset(auth->secret, 0, sizeof(auth->secret));
    memset(auth->key, 0, sizeof(auth->key));
    memcpy((char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key));
    meta->last += sizeof(tls_tunnel_auth_t);


    cup->ev->read_cb = NULL;
    cup->ev->write_cb = tls_tunnel_c_auth_send;
    return cup->ev->write_cb(cup);
}

static int tls_tunnel_c_connect_ssl(con_t * cup)
{    
    tls_tunnel_session_t * ses = cup->data;

    if(!cup->ssl) {
        if(0 != ssl_create_connection(cup, L_SSL_CLIENT)) {
            err("tls tunnel c. ssl create err\n");
            net_free(cup);
            net_free(ses->cdown);
            return -1;
        }
    }

    if(cup->ssl->f_err) {
        err("tls tunnel c. ssl handshake error\n");
        net_free(cup);
        net_free(ses->cdown);
        return -1;
    }

    if(!cup->ssl->f_handshaked) {
        cup->ssl->handshake_cb = tls_tunnel_c_connect_ssl;

        int rc = ssl_handshake(cup);
        if(rc < 0) {
            if(rc == -11) {
                tm_add(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshake failed\n");
            net_free(cup);
            net_free(ses->cdown);
            return -1;
        }
    }
    
    cup->recv = ssl_read;
    cup->send = ssl_write;
    cup->send_chain = ssl_write_chain;

    cup->ev->read_cb = NULL;
    cup->ev->write_cb = tls_tunnel_c_auth_build;
    return cup->ev->write_cb(cup);
}


static int tls_tunnel_c_connect_chk(con_t * cup)
{
    tls_tunnel_session_t * ses = cup->data;

    tm_del(cup);

    if(0 != net_socket_check_status(cup->fd)) {
        err("tls tunnel. socket chk err\n");
        net_free(cup);
        net_free(ses->cdown);
        return -1;
    }

    cup->ev->write_cb = tls_tunnel_c_connect_ssl;
    return cup->ev->write_cb(ses->cup);
}


int tls_tunnel_c_accept(con_t * cdown)
{
    tls_tunnel_session_t * ses = NULL;

    tm_del(cdown);

    if(!cdown->meta) {
        if(0 != meta_alloc(&cdown->meta, TLS_TUNNEL_METAN)) {   
            err("TLS tunnel c. alloc down meta.\n");
            net_free(cdown);
            return -1;
        }
    }

    if(0 != tls_ses_alloc(&ses)) {
        net_free(cdown);
        return -1;
    }

    ses->cdown = cdown;
    cdown->data = ses;
    cdown->data_cb = tls_ses_release_cdown;
    
    cdown->ev->read_cb = tls_tunnel_c_recv;
    cdown->ev->write_cb = NULL;


    if(0 != net_alloc(&ses->cup)) {
        net_free(cdown);
        return -1;
    }

    ses->cup->data = ses;
    ses->cup->data_cb = tls_ses_release_cup;
    if(!ses->cup->meta) {
        if(0 != meta_alloc(&ses->cup->meta, TLS_TUNNEL_METAN)) {
            err("TLS tunnel c. alloc up meta.\n");
            net_free(ses->cup);
           	net_free(cdown);
            return -1;
        }
    }

    ses->cup->ev->read_cb = NULL;
    ses->cup->ev->write_cb = tls_tunnel_c_connect_chk;


    tls_tunnel_s_addr(&ses->cup->addr);
    int rc = net_connect(ses->cup, &ses->cup->addr);
    if(rc < 0) {
        if(rc == -11) {
            tm_add(ses->cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel cup connect failed\n");
        net_free(ses->cup);
        net_free(cdown);
        return -1;
    }
    return ses->cup->ev->write_cb(ses->cup);
}

int tls_tunnel_c_init(void) { return 0; }

int tls_tunnel_c_exit(void) { return 0; }



#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"


static int tls_tunnel_c_recv(con_t * c) {
    /// cache read data
    tls_tunnel_session_t * ses = c->data;
    meta_t * meta = c->meta;
    int readn = 0;

    for(;;) {
        if(meta_getfree(meta) < 1) {
            err("TLS tunnel cdown recv ccdata too much\n");
            tls_ses_free(ses);
            return -1;
        }

        /// cache read data
        readn = c->recv(c, meta->last, meta_getfree(meta));
        if(readn < 0) {
            if(readn == -11)
                return -11;

            err("TLS tunnel cdown recv err\n");
            tls_ses_free(ses);
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

static int tls_tunnel_c_auth_send(con_t * c)
{
    tls_tunnel_session_t * ses = c->data;
    meta_t * meta = c->meta;

    int sendn = c->send_chain(c, meta);
    if(sendn < 0) {
        if(sendn == -11) {
            tm_add(c, tls_ses_exp, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel auth req send err\n");
        tls_ses_free(ses);
        return -1;
    }
    tm_del(c);
    meta_clr(meta);
    
    ses->cdown->ev->read_cb = tls_tunnel_traffic_proc;
    ses->cdown->ev->write_cb = NULL;
    return ses->cdown->ev->read_cb(ses->cdown);
}

static int tls_tunnel_c_auth_build(con_t * c)
{
    meta_t * meta = c->meta;

    net_socket_nodelay(c->fd);

    ///build auth req
    meta_clr(meta);
    tls_tunnel_auth_t * auth = (tls_tunnel_auth_t*)meta->last;
    auth->magic = htonl(TLS_TUNNEL_AUTH_MAGIC_NUM);
    memset(auth->secret, 0, sizeof(auth->secret));
    memset(auth->key, 0, sizeof(auth->key));
    memcpy((char*)auth->key, config_get()->s5_local_auth, sizeof(auth->key));
    meta->last += sizeof(tls_tunnel_auth_t);

    c->ev->read_cb = NULL;
    c->ev->write_cb = tls_tunnel_c_auth_send;
    return c->ev->write_cb(c);
}

static int tls_tunnel_c_connect_chk(con_t * c)
{    
    tls_tunnel_session_t * ses = c->data;
    tm_del(c);

    if(!c->ssl) schk(0 == ssl_create_connection(c, L_SSL_CLIENT), {tls_ses_free(ses);return -1;});

    if(!c->ssl->f_handshaked) {
        int rc = ssl_handshake(c);
        if(rc < 0) {
            if(rc == -11) {
                tm_add(c, tls_ses_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshake failed\n");
            tls_ses_free(ses);
            return -1;
        }
    }
    
    c->recv = ssl_read;
    c->send = ssl_write;
    c->send_chain = ssl_write_chain;

    c->ev->read_cb = NULL;
    c->ev->write_cb = tls_tunnel_c_auth_build;
    return c->ev->write_cb(c);
}


int tls_tunnel_c_accept(con_t * c)
{
    if(!c->meta) schk(0 == meta_alloc(&c->meta, TLS_TUNNEL_METAN), {net_free(c);return -1;});
    c->ev->read_cb = tls_tunnel_c_recv;
    c->ev->write_cb = NULL;
    
    tls_tunnel_session_t * ses = NULL;
    schk(0 == tls_ses_alloc(&ses), {net_free(c);return -1;});
    c->data = ses;
    ses->typ = TLS_TUNNEL_C;
    ses->cdown = c;

    schk(0 == net_alloc(&ses->cup), {tls_ses_free(ses);return -1;});
    ses->cup->data = ses;
    schk(0 == meta_alloc(&ses->cup->meta, TLS_TUNNEL_METAN), {tls_ses_free(ses);return -1;});

    ses->cup->ev->read_cb = NULL;
    ses->cup->ev->write_cb = tls_tunnel_c_connect_chk;
    
    tls_tunnel_s_addr(&ses->cup->addr);
    int rc = net_connect(ses->cup, &ses->cup->addr);
    if(rc < 0) {
        if(rc == -11) {
               tm_add(c, tls_ses_exp, TLS_TUNNEL_TMOUT);
            return -11;
        }
        err("TLS tunnel cup connect failed\n");
        tls_ses_free(ses);
        return -1;
    }
    return ses->cup->ev->write_cb(ses->cup);
}

int tls_tunnel_c_init(void)
{
    return 0;
}

int tls_tunnel_c_exit(void)
{
    return 0;
}



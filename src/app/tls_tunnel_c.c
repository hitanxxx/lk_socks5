#include "common.h"
#include "tls_tunnel_c.h"
#include "dns.h"
#include "tls_tunnel_s.h"

static int tls_tunnel_c_recv_in_cache(con_t *cdown) {
    /// cache read data
    tls_tunnel_session_t *session = cdown->user_data;
    meta_t *meta = cdown->meta;
    int readn = 0;

    for (;;) {
        if (meta_getfree(meta) < 1) {
            err("TLS tunnel cdown recv ccdata too much\n");
            net_free(session->cup);
            net_free(session->cdown);
            return -1;
        }

        /// cache read data
        readn = cdown->recv(cdown, meta->last, meta_getfree(meta));
        if (readn < 0) {
            if (readn == -11) {
                return -11;
            }
            ///do not print the error message. handle them silently
            ///err("TLS tunnel c. cdown recv in cache err\n");
            net_free(session->cup);
            net_free(session->cdown);
            return -1;
        }
        meta->last += readn;
    }
}

static inline void tls_tunnel_s_addr(struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(config_get()->s5_local_serv_port);
    addr->sin_addr.s_addr = inet_addr(config_get()->s5_local_serv_ip);
    return;
}

static int tls_tunnel_c_auth_send(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;
    meta_t *meta = cup->meta;

    int sendn = cup->send_chain(cup, meta);
    if (sendn < 0) {
        if (sendn == -11) {
            net_timer_add(cup, tls_session_timeout, TLS_TMOUT);
            return -11;
        }
        err("TLS tunnel auth req send err\n");
        net_free(cup);
        net_free(session->cdown);
        return -1;
    }

    net_timer_del(cup);
    meta_clr(meta);

    session->cdown->read_cb = tls_tunnel_traffic_proc;
    session->cdown->write_cb = NULL;
    return session->cdown->read_cb(session->cdown);
}

static int tls_tunnel_c_auth_build(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;
    
    if (!cup->meta) {
        if (0 != meta_alloc(&cup->meta, TLS_METAN)) {
            err("TLS tunnel c. alloc up meta.\n");
            net_free(cup);
            net_free(session->cdown);
            return -1;
        }
    }
    meta_t *meta = cup->meta;

    /// build auth req
    meta_clr(meta);
    meta_pnum(meta, TLS_AUTH_MG1);
    meta_pnum(meta, TLS_AUTH_MG2);
    meta_pnum(meta, strlen(config_get()->s5_local_auth));
    meta_pdata(meta, config_get()->s5_local_auth, strlen(config_get()->s5_local_auth));

    cup->read_cb = NULL;
    cup->write_cb = tls_tunnel_c_auth_send;
    return cup->write_cb(cup);
}

static int tls_tunnel_c_connect_ssl(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;

    if (!cup->ssl) {
        if (0 != net_ssl_create(cup, L_SSL_CLIENT)) {
            err("tls tunnel c. ssl create err\n");
            net_free(cup);
            net_free(session->cdown);
            return -1;
        }
    }

    if (net_ssl_check_err(cup)) {
        err("tls tunnel c. ssl handshake error\n");
        net_free(cup);
        net_free(session->cdown);
        return -1;
    }

    if (!net_ssl_check_handshaked(cup)) {
        int rc = net_ssl_handshake(cup);
        if (rc < 0) {
            if (rc == -11) {
                net_timer_add(cup, tls_session_timeout, TLS_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshake failed\n");
            net_free(cup);
            net_free(session->cdown);
            return -1;
        }
    }

    cup->read_cb = NULL;
    cup->write_cb = tls_tunnel_c_auth_build;
    return cup->write_cb(cup);
}

static int tls_tunnel_c_connect_chk(con_t *cup) {
    tls_tunnel_session_t *session = cup->user_data;

    net_timer_del(cup);

    if (0 != net_connect_chk(cup)) {
        err("tls tunnel. socket chk err\n");
        net_free(cup);
        net_free(session->cdown);
        return -1;
    }

    cup->read_cb = NULL;
    cup->write_cb = tls_tunnel_c_connect_ssl;
    return cup->write_cb(cup);
}

int tls_tunnel_c_accept(con_t *cdown) {
    net_timer_del(cdown);

    if (!cdown->meta) {
        if (0 != meta_alloc(&cdown->meta, TLS_METAN)) {
            err("TLS tunnel c. alloc down meta.\n");
            net_free(cdown);
            return -1;
        }
    }

    tls_tunnel_session_t *session = NULL;
    if (0 != tls_session_alloc(&session)) {
        err("TLS tunnel c. session alloc err\n");
        net_free(cdown);
        return -1;
    }
    session->cdown = cdown;
    cdown->user_data = session;
    cdown->free_user_data = tls_session_release_by_cdown;
    cdown->read_cb = tls_tunnel_c_recv_in_cache;
    cdown->write_cb = NULL;


    if (0 != net_alloc(&session->cup)) {
        err("TLS tunnel c. net alloc err\n");
        net_free(cdown);
        return -1;
    }
    session->cup->user_data = session;
    session->cup->free_user_data = tls_session_release_by_cup;
    session->cup->read_cb = tls_tunnel_c_connect_chk;
    session->cup->write_cb = tls_tunnel_c_connect_chk;
    

    tls_tunnel_s_addr(&session->cup->addr);
    int rc = net_connect(session->cup, NULL, 1);
    if (rc < 0) {
        if (rc == -11) {
            net_timer_add(session->cup, tls_session_timeout, TLS_TMOUT);
        } else {
            err("TLS tunnel cup connect failed\n");
            net_free(session->cup);
            net_free(cdown);
            return -1;
        }
    }
    
    return cdown->read_cb(cdown);
}

int tls_tunnel_c_init(void) { 
    if (config_get()->s5_mode == TLS_TUNNEL_C) {
        struct sockaddr_in addr;
        memset(&addr, 0x0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_get()->s5_local_port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        net_listen(tls_tunnel_c_accept,    &addr, 0);
    }
    return 0;
}

int tls_tunnel_c_exit(void) { return 0; }

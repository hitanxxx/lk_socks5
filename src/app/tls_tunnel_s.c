#include "common.h"
#include "dns.h"
#include "tls_tunnel_s.h"
#include "socks5.h"
#include "tls_tunnel_c.h"

#define TLS_TUNNEL_AUTH_FILE_MAX (4 * 1024)

typedef struct {
    ezac_ctx_t *ac;
} tls_tunnel_s_t;
static tls_tunnel_s_t *g_ses_ctx = NULL;

static int tls_tunnel_traffic_recv(con_t *c);
static int tls_tunnel_traffic_send(con_t *c);

static int tls_tunnel_traffic_reverse_recv(con_t *c);
static int tls_tunnel_traffic_reverse_send(con_t *c);

int tls_session_alloc(tls_tunnel_session_t **session) {
    tls_tunnel_session_t *alloc_session = mem_pool_alloc(sizeof(tls_tunnel_session_t));
    if (alloc_session) {
        *session = alloc_session;
        return 0;
    } 
    err("tls tunnel session alloc err\n");
    return -1;
}

void tls_session_release_by_cdown(void *data) {
    tls_tunnel_session_t *session = data;

    if (session->adata) {
        mem_pool_free(session->adata);
        session->adata = NULL;
    }
    if (session->dns) {
        dns_resolve_free(session->dns);
        session->dns = NULL;
    }
    mem_pool_free(session);
}

void tls_session_release_by_cup(void *data) {
    /// do nothing
    return;
}

void tls_session_timeout(ev_timer_t *timer) {
    con_t *c = ev_timer_userdata(timer);
    tls_tunnel_session_t *session = c->user_data;

    if (session->cup) {
        net_free(session->cup);
    }
    
    if (session->cdown) {
        net_free(session->cdown);
    }
    return;
}

static int tls_tunnel_traffic_recv(con_t *c) {
    tls_tunnel_session_t *session = c->user_data;
    con_t *cdown = session->cdown;
    con_t *cup = session->cup;
    int recvn = 0;

    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
    net_timer_add(cup, tls_session_timeout, TLS_TMOUT);

    for (;;) {
        while (meta_getfree(cdown->meta) > 0) {
            recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    session->frecv_err_down = 1;
                } else if (recvn == -11) {
                    if (meta_getlen(cdown->meta) < 1) {
                        net_ev_set(cup, net_ev_clr(cup, EV_W));
                        net_ev_set(cdown, net_ev_add(cdown, EV_R));
                        return -11;
                    }
                }
                break;
            }
            cdown->meta->last += recvn;
        }
        
        while (meta_getlen(cdown->meta) > 0) {
            int sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
            if (sendn < 0) {
                if (sendn == -11) {
                    net_ev_set(cup, net_ev_add(cup, EV_W));
                    net_ev_set(cdown, net_ev_clr(cdown, EV_R));
                    return -11;
                    
                } else if (sendn == -1) {
                    dbg("tls tunnel teminate. (up send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
            }
            cdown->meta->pos += sendn;
        }
        
        if (session->frecv_err_down) {
            dbg("tls tunnel teminate. (cdown recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cdown->meta);
    }
    return -11;
}

static int tls_tunnel_traffic_send(con_t *c) {
    tls_tunnel_session_t *session = c->user_data;
    con_t *cdown = session->cdown;
    con_t *cup = session->cup;
    int sendn = 0;

    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
    net_timer_add(cup, tls_session_timeout, TLS_TMOUT);

    for (;;) {
        while (meta_getlen(cdown->meta) > 0) {
            sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
            if (sendn < 0) {
                if (sendn == -1) {
                    dbg("tls tunnel teminate. (cup send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
                return -11;
            }
            cdown->meta->pos += sendn;
        }
        
        if (session->frecv_err_down) {
            dbg("tls tunnel teminate. (cdown recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cdown->meta);

        while (meta_getfree(cdown->meta) > 0) {
            int recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    session->frecv_err_down = 1;
                    if (meta_getlen(cdown->meta) < 1) {
                        dbg("tls tunnel teminate. (cdown recv)\n");
                        net_free(cup);
                        net_free(cdown);
                        return -1;
                    }
                } else if (recvn == -11) {
                    if (meta_getlen(cdown->meta) < 1) {
                        net_ev_set(cup, net_ev_clr(cup, EV_W));
                        net_ev_set(cdown, net_ev_add(cdown, EV_R));
                        return -11;
                    }
                }
                break;
            }
            cdown->meta->last += recvn;
        }
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_recv(con_t *c) {
    tls_tunnel_session_t *session = c->user_data;
    con_t *cdown = session->cdown;
    con_t *cup = session->cup;
    int recvn = 0;

    net_timer_add(cup, tls_session_timeout, TLS_TMOUT);
    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);

    for (;;) {
        while (meta_getfree(cup->meta) > 0) {
            recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    session->frecv_err_down = 1;
                } else if (recvn == -11) {
                    if (meta_getlen(cup->meta) < 1) {
                        net_ev_set(cdown, net_ev_clr(cdown, EV_W));
                        net_ev_set(cup, net_ev_add(cup, EV_R));
                        return -11;
                    }
                }
                break;
            }
            cup->meta->last += recvn;
        }
        
        while (meta_getlen(cup->meta) > 0) {
            int sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
            if (sendn < 0) {
                if (sendn == -11) {
                    net_ev_set(cdown, net_ev_add(cdown, EV_W));
                    net_ev_set(cup, net_ev_clr(cup, EV_R));
                    return -11;
                } else if (sendn == -1) {
                    dbg("tls tunnel teminate. (cdown send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
            }
            cup->meta->pos += sendn;
        }

        if (session->frecv_err_down) {
            dbg("tls tunnel teminate. (cup recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cup->meta);
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_send(con_t *c) {
    tls_tunnel_session_t *session = c->user_data;
    con_t *cdown = session->cdown;
    con_t *cup = session->cup;
    int sendn = 0;

    net_timer_add(cup, tls_session_timeout, TLS_TMOUT);
    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);

    for (;;) {
        while (meta_getlen(cup->meta) > 0) {
            sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
            if (sendn < 0) {
                if (sendn == -1) {
                    dbg("tls tunnel teminate. (cdown send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
                return -11;
            }
            cup->meta->pos += sendn;
        }
        
        if (session->frecv_err_down) {
            err("tls tunnel teminate. (cup recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cup->meta);

        while (meta_getfree(cup->meta) > 0) {
            int recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    session->frecv_err_down = 1;
                    if (meta_getlen(cup->meta) < 1) {
                        dbg("tls tunnel teminate. (cup recv)\n");
                        net_free(cup);
                        net_free(cdown);
                        return -1;
                    }
                } else if (recvn == -11) {
                    if (meta_getlen(cup->meta) < 1) {
                        net_ev_set(cdown, net_ev_clr(cdown, EV_W));
                        net_ev_set(cup, net_ev_add(cup, EV_R));
                        return -11;
                    }
                }
                break;
            }
            cup->meta->last += recvn;
        }
    }
    return -11;
}

int tls_tunnel_traffic_proc(con_t *c) {
    tls_tunnel_session_t *session = c->user_data;
    con_t *cdown = session->cdown;
    con_t *cup = session->cup;

    if (!cdown->meta) {
        if (0 != meta_alloc(&cdown->meta, TLS_METAN)) {
            err("tls tunnel. cdown meta alloc\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
    }
    if (!cup->meta) {
        if (0 != meta_alloc(&cup->meta, TLS_METAN)) {
            err("tls tunnel. cup meta alloc\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
    }
    /// only clear up meta in here. because local run in here too.
    /// local(down) mabey recv some data.
    meta_clr(cup->meta);

    cdown->read_cb = tls_tunnel_traffic_recv;
    cup->write_cb = tls_tunnel_traffic_send;

    cup->read_cb = tls_tunnel_traffic_reverse_recv;
    cdown->write_cb = tls_tunnel_traffic_reverse_send;

    net_ev_set(cdown, EV_R);
    net_ev_set(cup, EV_R);

    if (meta_getlen(cdown->meta) > 0) {
        return cup->write_cb(cup);
    } else {
        return cdown->read_cb(cdown);
    }
}

static int tls_tunnel_s_auth_chk(con_t *cdown) {
    tls_tunnel_session_t *session = cdown->user_data;

    enum { s_mg1 = 0, s_mg2, s_len, s_data };

    for (;;) {
        if (meta_getlen(cdown->meta) < 1) {
            int recvd = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvd < 0) {
                if (recvd == -11) {
                    net_timer_add(cdown, tls_session_timeout, TLS_TMOUT);
                    return -11;
                }
                err("webreq recv err\n");
                return -1;
            }
            cdown->meta->last += recvd;
        }

        uint8_t *p = NULL;
        for (; cdown->meta->pos < cdown->meta->last; cdown->meta->pos++) {
            p = cdown->meta->pos;
            if (session->auth_state == s_mg1) {
                if (*p != TLS_AUTH_MG1) {
                    err("TLS auth chk. mg1 err\n");
                    net_free(cdown);
                    return -1;
                }
                session->auth_state = s_mg2;
            } else if (session->auth_state == s_mg2) {
                if (*p != TLS_AUTH_MG2) {
                    err("TLS auth chk. mg2 err\n");
                    net_free(cdown);
                    return -1;
                }
                session->auth_state = s_len;
            } else if (session->auth_state == s_len) {
                session->auth_data_all = *p;
                if (session->auth_data_all < 1 || session->auth_data_all > 31) {
                    err("TLS auth chk. slen [%d] illegal\n", session->auth_data_all);
                    net_free(cdown);
                    return -1;
                }
                session->auth_state = s_data;
            } else if (session->auth_state == s_data) {
                session->auth_data[session->auth_data_recv++] = *p;
                if (session->auth_data_recv == session->auth_data_all) {
                    if (0 == ezac_find(g_ses_ctx->ac, (char*)session->auth_data, session->auth_data_all)) {
                        net_timer_del(cdown);

                        meta_clr(cdown->meta);

                        cdown->read_cb = s5_p1_req;
                        cdown->write_cb = NULL;
                        return cdown->read_cb(cdown);
                    } else {
                        err("TLS auth chk. auth not found\n");
                        net_free(cdown);
                        return -1;
                    }
                }
            }
        }
    }

    err("TLS auth chk not fin. auth_state [%d]\n", session->auth_state);
    net_free(cdown);
    return -1;
}

int tls_tunnel_s_start(con_t *cdown) {
    net_timer_del(cdown);
    
    if (!cdown->meta) {
        schk(0 == meta_alloc(&cdown->meta, TLS_METAN), {
            net_free(cdown);
            return -1;
        });
    }

    tls_tunnel_session_t *session = NULL;
    schk(0 == tls_session_alloc(&session), {
        net_free(cdown);
        return -1;
    });
    session->cdown = cdown;
    cdown->user_data = session;
    cdown->free_user_data = tls_session_release_by_cdown;
    

    session->atyp = 0; /// s5
    if (session->atyp == 0) {
        session->adata = mem_pool_alloc(sizeof(s5_t));
        if (!session->adata) {
            err("tls tunnel. alloc s5 err\n");
            net_free(cdown);
            return -1;
        }
    }

    cdown->read_cb = tls_tunnel_s_auth_chk;
    cdown->write_cb = NULL;
    return cdown->read_cb(cdown);
}

int tls_tunnel_s_accept(con_t *cdown) {
    net_timer_del(cdown);

    if (!cdown->ssl) {
        if (0 != net_ssl_create(cdown, L_SSL_SERVER)) {
            err("tls tunnel. cdown ssl create err\n");
            net_free(cdown);
            return -1;
        }
    }

    if (net_ssl_check_err(cdown)) {
        err("tls tunnel. cdown handshake error\n");
        net_free(cdown);
        return -1;
    }

    if (!net_ssl_check_handshaked(cdown)) {
        int rc = net_ssl_handshake(cdown);
        if (rc < 0) {
            if (rc == -11) {
                net_timer_add(cdown, net_free_timeout, TLS_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshek err\n");
            net_free(cdown);
            return -1;
        }
    }

    cdown->read_cb = tls_tunnel_s_start;
    cdown->write_cb = NULL;
    return cdown->read_cb(cdown);
}

static int tls_tunnel_s_auth_mgr_fparse(char *data) {
    cJSON *root = cJSON_Parse(data);
    if (root) { /// traversal the array
        int i = 0;
        for (i = 0; i < cJSON_GetArraySize(root); i++) {
            cJSON *arrobj = cJSON_GetArrayItem(root, i);
            if (0 != ezac_add(g_ses_ctx->ac, cJSON_GetStringValue(arrobj),
                              strlen(cJSON_GetStringValue(arrobj)))) {
                err("s5 srv auth add ac err\n", cJSON_GetStringValue(arrobj));
            }
        }
        cJSON_Delete(root);
    }
    return 0;
}

static int tls_tunnel_s_auth_mgr_init(void) {
    schk(g_ses_ctx->ac = ezac_init(), return -1);
    sys_data_t *fdata = sys_file_read_value(config_get()->s5_serv_auth_path);
    if (fdata) {
        tls_tunnel_s_auth_mgr_fparse(fdata->data);
        ezac_compiler(g_ses_ctx->ac);
        sys_free(fdata);
    }
    return 0;
}

int tls_tunnel_s_init(void) {
    g_ses_ctx = (tls_tunnel_s_t *)mem_pool_alloc(sizeof(tls_tunnel_s_t));
    if (g_ses_ctx) {
        if (config_get()->s5_mode == TLS_TUNNEL_S ||
            config_get()->s5_mode == TLS_TUNNEL_S_SCRECT) {
            schk(tls_tunnel_s_auth_mgr_init() == 0, return -1);

            struct sockaddr_in addr;
            memset(&addr, 0x0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(config_get()->s5_serv_port);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            
            net_listen(tls_tunnel_s_accept,    &addr, 1);
        }
    }
    return 0;
}

int tls_tunnel_s_exit(void) {
    if (g_ses_ctx) {
        if (g_ses_ctx->ac)
            ezac_free(g_ses_ctx->ac);

        mem_pool_free((void *)g_ses_ctx);
        g_ses_ctx = NULL;
    }
    return 0;
}

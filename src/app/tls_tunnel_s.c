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

int tls_ses_alloc(tls_tunnel_session_t **ses) {
    tls_tunnel_session_t *nses = NULL;
    schk(nses = mem_pool_alloc(sizeof(tls_tunnel_session_t)), return -1);
    *ses = nses;
    return 0;
}

void tls_ses_release_cdown(void *data) {
    tls_tunnel_session_t *ses = data;

    if (ses->adata) {
        mem_pool_free(ses->adata);
        ses->adata = NULL;
    }
    mem_pool_free(ses);
}

void tls_ses_release_cup(void *data) {
    /// do nothing
    return;
}

void tls_ses_exp(void *data) {
    con_t *c = data;
    tls_tunnel_session_t *ses = c->data;

    if (ses->cup) {
        if (ses->cup->fssl && ses->cup->ssl) {
            ses->cup->ssl->f_err = 1;
        }
        net_free(ses->cup);
    }
    
    if (ses->cdown) {
        if (ses->cdown->fssl && ses->cdown->ssl) {
            ses->cdown->ssl->f_err = 1;
        }
        net_free(ses->cdown);
    }
    return;
}

static int tls_tunnel_traffic_recv(con_t *c) {
    tls_tunnel_session_t *ses = c->data;
    con_t *cdown = ses->cdown;
    con_t *cup = ses->cup;
    int recvn = 0;

    EZ_TMADD(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
    EZ_TMADD(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);

    for (;;) {
        ///try to recv 
        while (meta_getfree(cdown->meta) > 0) {
            recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    ses->frecv_err_down = 1;
                } else if (recvn == -11) {
                    ///meta empty. kernel empty
                    if (meta_getlen(cdown->meta) < 1) {
                        return -11;
                    }
                }
                break;
                ///meta not full. kernel empty. next time will call read cb if kernel have data
            }
            cdown->meta->last += recvn;
        }
        ///meta full. don't know kernel empty or not 
        
        while (meta_getlen(cdown->meta) > 0) {
            int sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
            if (sendn < 0) {
                if (sendn == -11) {
                    ev_opt(cup, EV_W | cup->ev->opt);
                    return -11;
                    ///meta remain data. but kernel full. next time will call write cb if kernel have space
                } else if (sendn == -1) {
                    err("tls tunnel teminate. (up send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
            }
            cdown->meta->pos += sendn;
        }
        
        ///if send all over. try recv again. to know kernel empty or not 
        if (ses->frecv_err_down) {
            ///if recv error. free session
            err("tls tunnel teminate. (cdown recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cdown->meta);
    }
    return -11;
}

static int tls_tunnel_traffic_send(con_t *c) {
    tls_tunnel_session_t *ses = c->data;
    con_t *cdown = ses->cdown;
    con_t *cup = ses->cup;
    int sendn = 0;

    EZ_TMADD(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
    EZ_TMADD(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);

    for (;;) {
        ///try to send 
        while (meta_getlen(cdown->meta) > 0) {
            sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
            if (sendn < 0) {
                if (sendn == -1) {
                    err("tls tunnel teminate. (cup send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
                ///meta remain data. but kernel full. next time will call write cb if kernel have space
                return -11;
            }
            cdown->meta->pos += sendn;
        }
        ///meta empty.
        meta_clr(cdown->meta);

        ///try to recv again. don't know kernel is empty or not 
        while (meta_getfree(cdown->meta) > 0) {
            int recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    ses->frecv_err_down = 1;
                } else if (recvn == -11) {
		    ///do nothing
                }
                break;
            }
            cdown->meta->last += recvn;
        }

        ///meta empty. back to read cb(kernel must be empty. becasue have manual recv before this)
        if (meta_getlen(cdown->meta) < 1) {
            ///if recv error. free session
            if (ses->frecv_err_down) {
                err("tls tunnel teminate. (cdown recv)\n");
                net_free(cup);
                net_free(cdown);
                return -1;
            }
            ev_opt(cup, cup->ev->opt & ~(EV_W));
            break;
        }
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_recv(con_t *c) {
    tls_tunnel_session_t *ses = c->data;
    con_t *cdown = ses->cdown;
    con_t *cup = ses->cup;
    int recvn = 0;

    EZ_TMADD(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
    EZ_TMADD(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);

    for (;;) {
        ///try to recv 
        while (meta_getfree(cup->meta) > 0) {
            recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    ses->frecv_err_down = 1;
                } else if (recvn == -11) {
                    ///meta empty. kernel empty
                    if (meta_getlen(cup->meta) < 1) 
                        return -11;
                }
                break;
                ///meta not full. kernel empty. next time will call read cb if kernel have data
            }
            cup->meta->last += recvn;
        }
        ///meta full. don't know kernel empty or not 
        
        while (meta_getlen(cup->meta) > 0) {
            int sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
            if (sendn < 0) {
                if (sendn == -11) {
                    ev_opt(cdown, EV_W | cdown->ev->opt);
                    return -11;
                    ///meta remain data. but kernel full. next time will call write cb if kernel have space
                } else if (sendn == -1) {
                    err("tls tunnel teminate. (cdown send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
            }
            cup->meta->pos += sendn;
        }

        ///if send all over. try recv again. to know kernel empty or not 
        if (ses->frecv_err_down) {
            ///if error. free session
            err("tls tunnel teminate. (cup recv)\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
        meta_clr(cup->meta);
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_send(con_t *c) {
    tls_tunnel_session_t *ses = c->data;
    con_t *cdown = ses->cdown;
    con_t *cup = ses->cup;
    int sendn = 0;

    EZ_TMADD(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);
    EZ_TMADD(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);

    for (;;) {
        ///try to send 
        while (meta_getlen(cup->meta) > 0) {
            sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
            if (sendn < 0) {
                if (sendn == -1) {
                    err("tls tunnel teminate. (by cdown send)\n");
                    net_free(cup);
                    net_free(cdown);
                    return -1;
                }
                ///meta remain data. but kernel full. next time will call write cb if kernel have space
                return -11;
            }
            cup->meta->pos += sendn;
        }
        ///meta empty
        meta_clr(cup->meta);

        ///try to recv again. don't know kernel is empty or not 
        while (meta_getfree(cup->meta) > 0) {
            int recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
            if (recvn < 0) {
                if (recvn == -1) {
                    ses->frecv_err_down = 1;
                } else if (recvn == -11) {
		    ///do nothing
                }
                break; /// break when -11 (EAGAIN) or -1
            }
            cup->meta->last += recvn;
        }

        ///meta empty. back to read cb(kernel must be empty. becasue have manual recv before this)
        if (meta_getlen(cup->meta) < 1) {
            ///if recv error. free session
            if (ses->frecv_err_down) {
                err("tls tunnel teminate. (by cup recv)\n");
                net_free(cup);
                net_free(cdown);
                return -1;
            }
            ev_opt(cdown, cdown->ev->opt & ~(EV_W));
            break;
        }
    }
    return -11;
}

int tls_tunnel_traffic_proc(con_t *c) {
    tls_tunnel_session_t *ses = c->data;
    con_t *cdown = ses->cdown;
    con_t *cup = ses->cup;

    if (!cdown->meta) {
        if (0 != meta_alloc(&cdown->meta, TLS_TUNNEL_METAN)) {
            err("tls tunnel. cdown meta alloc\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
    }
    if (!cup->meta) {
        if (0 != meta_alloc(&cup->meta, TLS_TUNNEL_METAN)) {
            err("tls tunnel. cup meta alloc\n");
            net_free(cup);
            net_free(cdown);
            return -1;
        }
    }
    /// only clear up meta in here. because local run in here too.
    /// local(down) mabey recv some data.
    meta_clr(cup->meta);

    cdown->ev->read_cb = tls_tunnel_traffic_recv;
    cup->ev->write_cb = tls_tunnel_traffic_send;

    cup->ev->read_cb = tls_tunnel_traffic_reverse_recv;
    cdown->ev->write_cb = tls_tunnel_traffic_reverse_send;

    ev_opt(cdown, EV_R);
    ev_opt(cup, EV_R);

    if (meta_getlen(cdown->meta) > 0) {
        return cup->ev->write_cb(cup);
    } else {
        return cdown->ev->read_cb(cdown);
    }
}

static int tls_tunnel_s_auth_chk(con_t *cdown) {
    tls_tunnel_session_t *ses = cdown->data;

    enum { s_mg1 = 0, s_mg2, s_len, s_data };

    for (;;) {
        if (meta_getlen(cdown->meta) < 1) {
            int recvd = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
            if (recvd < 0) {
                if (recvd == -11) {
                    EZ_TMADD(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);
                    return -11;
                }
                err("webreq recv err\n");
                return -1;
            }
            cdown->meta->last += recvd;
        }

        unsigned char *p = NULL;
        for (; cdown->meta->pos < cdown->meta->last; cdown->meta->pos++) {
            p = cdown->meta->pos;
            if (ses->state == s_mg1) {
                if (*p != TLS_AUTH_MG1) {
                    err("TLS auth chk. mg1 err\n");
                    net_free(cdown);
                    return -1;
                }
                ses->state = s_mg2;
            } else if (ses->state == s_mg2) {
                if (*p != TLS_AUTH_MG2) {
                    err("TLS auth chk. mg2 err\n");
                    net_free(cdown);
                    return -1;
                }
                ses->state = s_len;
            } else if (ses->state == s_len) {
                ses->auth_data_all = *p;
                if (ses->auth_data_all < 1 || ses->auth_data_all > 31) {
                    err("TLS auth chk. slen [%d] illegal\n", ses->auth_data_all);
                    net_free(cdown);
                    return -1;
                }
                ses->state = s_data;
            } else if (ses->state == s_data) {
                ses->auth_data[ses->auth_data_recv++] = *p;
                if (ses->auth_data_recv == ses->auth_data_all) {
                    if (0 == ezac_find(g_ses_ctx->ac, (char*)ses->auth_data, ses->auth_data_all)) {
                        EZ_TMDEL(cdown);

                        meta_clr(cdown->meta);

                        cdown->ev->read_cb = s5_p1_req;
                        cdown->ev->write_cb = NULL;
                        return cdown->ev->read_cb(cdown);
                    } else {
                        err("TLS auth chk. auth not found\n");
                        net_free(cdown);
                        return -1;
                    }
                }
            }
        }
    }

    err("TLS auth chk not fin. state [%d]\n", ses->state);
    net_free(cdown);
    return -1;
}

int tls_tunnel_s_start(con_t *cdown) {
    tls_tunnel_session_t *ses = NULL;

    EZ_TMDEL(cdown);

    if (!cdown->meta) {
        schk(0 == meta_alloc(&cdown->meta, TLS_TUNNEL_METAN), {
            net_free(cdown);
            return -1;
        });
    }

    schk(0 == tls_ses_alloc(&ses), {
        net_free(cdown);
        return -1;
    });
    ses->cdown = cdown;

    cdown->data = ses;
    cdown->data_cb = tls_ses_release_cdown;

    ses->atyp = 0; /// s5
    if (ses->atyp == 0) {
        ses->adata = mem_pool_alloc(sizeof(s5_t));
        if (!ses->adata) {
            err("tls tunnel. alloc s5 err\n");
            net_free(cdown);
            return -1;
        }
    }

    cdown->ev->read_cb = tls_tunnel_s_auth_chk;
    cdown->ev->write_cb = NULL;
    return cdown->ev->read_cb(cdown);
}

int tls_tunnel_s_accept(con_t *cdown) {
    EZ_TMDEL(cdown);

    if (!cdown->ssl) {
        if (0 != ssl_create_connection(cdown, L_SSL_SERVER)) {
            err("tls tunnel. cdown ssl create err\n");
            net_free(cdown);
            return -1;
        }
        cdown->ssl->cc_ev_cbr = cdown->ev->read_cb;
        cdown->ssl->cc_ev_cbw = cdown->ev->write_cb;
        cdown->ssl->cc_ev_typ = cdown->ev->opt;
    }

    if (cdown->ssl->f_err) {
        err("tls tunnel. cdown handshake error\n");
        net_free(cdown);
        return -1;
    }

    if (!cdown->ssl->f_handshaked) {
        cdown->ssl->handshake_cb = tls_tunnel_s_accept;
        int rc = ssl_handshake(cdown);
        if (rc < 0) {
            if (rc == -11) {
                EZ_TMADD(cdown, net_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshek err\n");
            net_free(cdown);
            return -1;
        }
    }

    cdown->recv = ssl_read;
    cdown->send = ssl_write;
    cdown->send_chain = ssl_write_chain;

    cdown->ev->read_cb = tls_tunnel_s_start;
    cdown->ev->write_cb = NULL;
    return cdown->ev->read_cb(cdown);
}

static int tls_tunnel_s_auth_mgr_fparse(meta_t *meta) {
    cJSON *root = cJSON_Parse((char *)meta->pos);
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

static int tls_tunnel_s_auth_mgr_fread(meta_t *meta) {
    ssize_t size = 0;
    int fd = open((char *)config_get()->s5_serv_auth_path, O_RDONLY);
    schk(fd > 0, return -1);
    size = read(fd, meta->pos, meta_getfree(meta));
    close(fd);
    schk(size != -1, return -1);
    meta->last += size;
    return 0;
}

static int tls_tunnel_s_auth_mgr_init(void) {
    meta_t *meta = NULL;
    int rc = -1;
    do {
        schk(g_ses_ctx->ac = ezac_init(), break);
        schk(meta_alloc(&meta, TLS_TUNNEL_AUTH_FILE_MAX) == 0, break);
        schk(tls_tunnel_s_auth_mgr_fread(meta) == 0, break);
        schk(tls_tunnel_s_auth_mgr_fparse(meta) == 0, break);
        ezac_compiler(g_ses_ctx->ac);
        rc = 0;
    } while (0);

    if (meta)
        meta_free(meta);
    return rc;
}

int tls_tunnel_s_init(void) {
    schk(!g_ses_ctx, return -1);
    schk(g_ses_ctx = (tls_tunnel_s_t *)mem_pool_alloc(sizeof(tls_tunnel_s_t)),
         return -1);
    if (config_get()->s5_mode > TLS_TUNNEL_C) {
        schk(tls_tunnel_s_auth_mgr_init() == 0, return -1);
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

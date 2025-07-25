#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"
#include "socks5.h"


#define TLS_TUNNEL_AUTH_FILE_MAX  (4*1024)

typedef struct {
    ezac_ctx_t * ac;    
} tls_tunnel_s_t;
static tls_tunnel_s_t * g_ses_ctx = NULL;


static int tls_tunnel_traffic_recv(con_t * c);
static int tls_tunnel_traffic_send(con_t * c);

static int tls_tunnel_traffic_reverse_recv(con_t * c);
static int tls_tunnel_traffic_reverse_send(con_t * c);



int tls_ses_alloc(tls_tunnel_session_t ** ses)
{
    tls_tunnel_session_t * nses = NULL;
    schk(nses = mem_pool_alloc(sizeof(tls_tunnel_session_t)), return -1);
    *ses = nses;
    return 0;
}

int tls_ses_free(tls_tunnel_session_t * ses)
{
    if(ses->cdown) net_free(ses->cdown);
    if(ses->cup) net_free(ses->cup);
    if(ses->adata) mem_pool_free(ses->adata);

    mem_pool_free(ses);
    return 0;
}

void tls_ses_exp(void * data)
{
    con_t * c = data;
    tls_tunnel_session_t * ses = c->data;
    tls_ses_free(ses);
}

static int tls_tunnel_traffic_recv(con_t * c)
{
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;    
    int recvn = 0;

    tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);

    while(meta_getfree(cdown->meta) > 0) {
        recvn = cdown->recv(cdown, cdown->meta->last, meta_getfree(cdown->meta));
        if(recvn < 0) {
            if(recvn == -1) {
                err("TLS tunnel <%s>. near => S5 => far. near recv err\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
                ses->frecv_err_down = 1;
            }
            break; ///break when -11 (EAGAIN)
        }
        cdown->meta->last += recvn;
    }

    if(meta_getlen(cdown->meta) > 0) {
        cdown->ev->read_cb = NULL;
        cup->ev->write_cb = tls_tunnel_traffic_send;
        return cup->ev->write_cb(cup);
    }
    if(ses->frecv_err_down) {
        tls_ses_free(ses);
        return -1;
    }
    return -11;
}

static int tls_tunnel_traffic_send(con_t * c)
{    
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int sendn = 0;

    tm_add(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);

    while(meta_getlen(cdown->meta) > 0) {
        sendn = cup->send(cup, cdown->meta->pos, meta_getlen(cdown->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("TLS tunnel <%s>. near => S5 => far. far send err\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
                tls_ses_free(ses);
                return -1;
            }
            return -11;
        }
        cdown->meta->pos += sendn;
    }

    if(ses->frecv_err_down) {
        err("TLS tunnel <%s>. near => S5 => far. near close notify\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
        tls_ses_free(ses);
        return -1;
    }
    meta_clr(cdown->meta);
    cup->ev->write_cb = NULL;
    cdown->ev->read_cb = tls_tunnel_traffic_recv;
    return cdown->ev->read_cb(cdown);
}

static int tls_tunnel_traffic_reverse_recv(con_t * c)
{
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int recvn = 0;

    tm_add(cup, tls_ses_exp, TLS_TUNNEL_TMOUT);

    while(meta_getfree(cup->meta) > 0) {
        recvn = cup->recv(cup, cup->meta->last, meta_getfree(cup->meta));
        if(recvn < 0) {
            if(recvn == -1) {   
                err("TLS tunnel <%s>. far => S5 => near. far recv err\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
                ses->frecv_err_up = 1;
            }
            break;
        }
        cup->meta->last += recvn;
    }

    if(meta_getlen(cup->meta) > 0) {
        cup->ev->read_cb = NULL;
        cdown->ev->write_cb = tls_tunnel_traffic_reverse_send;
        return cdown->ev->write_cb(cdown);
    }
    if(ses->frecv_err_up) {
        tls_ses_free(ses);
        return -1;
    }
    return -11;
}

static int tls_tunnel_traffic_reverse_send(con_t * c)
{
    tls_tunnel_session_t * ses = c->data;
    con_t * cdown = ses->cdown;
    con_t * cup = ses->cup;
    int sendn = 0;
    
    tm_add(cdown, tls_ses_exp, TLS_TUNNEL_TMOUT);

    while(meta_getlen(cup->meta) > 0) {
        sendn = cdown->send(cdown, cup->meta->pos, meta_getlen(cup->meta));
        if(sendn < 0) {
            if(sendn == -1) {
                err("TLS tunnel <%s>. far => S5 => near. near send err\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
                tls_ses_free(ses);
                return -1;
            }
            return -11;
        }
        cup->meta->pos += sendn;
    }
    if(ses->frecv_err_up) {
        err("TLS tunnel <%s>. far => S5 => near. far close notify\n", config_get()->s5_mode == TLS_TUNNEL_C ? "C" : "S");
        tls_ses_free(ses);
        return -1;
    }
    meta_clr(cup->meta);
    cdown->ev->write_cb = NULL;
    cup->ev->read_cb = tls_tunnel_traffic_reverse_recv;
    return cup->ev->read_cb(cup);
}

int tls_tunnel_traffic_proc(con_t * c)
{
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
    cdown->ev->read_cb = tls_tunnel_traffic_recv;
    cup->ev->write_cb = NULL;

    ///up -> down
    cup->ev->read_cb = tls_tunnel_traffic_reverse_recv;
    cdown->ev->write_cb = NULL;
    
    ev_opt(cdown, EV_R|EV_W);
    ev_opt(cup, EV_R|EV_W);    
    return cdown->ev->read_cb(cdown);
}

static int tls_tunnel_s_auth_chk(con_t * c)
{
    tls_tunnel_session_t * ses = c->data;
    meta_t * meta = c->meta;
    
    while(meta_getlen(meta) < sizeof(tls_tunnel_auth_t)) {
        int recvn = c->recv(c, meta->last, meta_getfree(meta));
        if(recvn < 0) {
            if(recvn == -11) {
                tm_add(c, tls_ses_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel auth recv failed\n");
            tls_ses_free(ses);
            return -1;
        }
        meta->last += recvn;
    }
    tm_del(c);

    ///chk auth hdr
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
    
    c->ev->read_cb = s5_p1_req;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

int tls_tunnel_s_start(con_t * c)
{
	tls_tunnel_session_t * ses = NULL;

	if(!c->meta) schk(0 == meta_alloc(&c->meta, TLS_TUNNEL_METAN), {net_free(c); return -1;});
    
    schk(0 == tls_ses_alloc(&ses), {net_free(c); return -1;});
    ses->cdown = c;
    c->data = ses;
        
    ses->atyp = 0;
    if(ses->atyp == 0) schk(ses->adata = mem_pool_alloc(sizeof(s5_t)), {tls_ses_free(ses);return -1;});
    
    c->ev->read_cb = tls_tunnel_s_auth_chk;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

int tls_tunnel_s_accept(con_t * c)
{
    if(!c->ssl) schk(0 == ssl_create_connection(c, L_SSL_SERVER), {net_free(c);return -1;});

    if(!c->ssl->f_handshaked) {
        int rc = ssl_handshake(c);
        if(rc < 0) {
            if(rc == -11) {
                tm_add(c, net_exp, TLS_TUNNEL_TMOUT);
                return -11;
            }
            err("TLS tunnel. handshek err\n");
            net_free(c);
            return -1;
        }
    }

    c->recv = ssl_read;
    c->send = ssl_write;
    c->send_chain = ssl_write_chain;

    c->ev->read_cb = tls_tunnel_s_start;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
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


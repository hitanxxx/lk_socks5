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
    
    if(ses->adata) 
        mem_pool_free(ses->adata);

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
    
    event_opt(ses->cdown->event, ses->cdown->fd, EV_R);
    event_opt(ses->cup->event, ses->cup->fd, EV_R);	
    return ses->cdown->event->read_pt(ses->cdown->event);
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
    ev->read_pt = s5_p1_req;
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
    ses->atyp = 0;
    if(ses->atyp == 0) {
        ses->adata = mem_pool_alloc(sizeof(s5_t));
        if(!ses->adata) {
            err("TLS tunnel alloc adata s5 err\n");
            tls_ses_free(ses);
            return -1;
        }
    }
    
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
    ses->atyp = 0;
    if(ses->atyp == 0) {
        ses->adata = mem_pool_alloc(sizeof(s5_t));
        if(!ses->adata) {
            err("TLS tunnel alloc adata s5 err\n");
            tls_ses_free(ses);
            return -1;
        }
    }
    
    ses->cdown = cdown;
    cdown->data = ses;

    ev->read_pt = tls_tunnel_s_auth_chk;
    return ev->read_pt(ev);
}

int tls_tunnel_s_accept_chk(event_t * ev)
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
#if defined(S5_SINGLE)
    if(!cdown->meta)
        schk(0 == meta_alloc(&cdown->meta, TLS_TUNNEL_METAN), {net_free(cdown); return -1;});

    tls_tunnel_session_t * ses = NULL;
    schk(0 == tls_ses_alloc(&ses), {net_free(cdown); return -1;});
    
    ses->atyp = 0;
    if(ses->atyp == 0) {
        ses->adata = mem_pool_alloc(sizeof(s5_t));
        if(!ses->adata) {
            err("TLS tunnel alloc adata s5 err\n");
            tls_ses_free(ses);
            return -1;
        }
    }

    ses->cdown = cdown;
    cdown->data = ses;

    ev->write_pt = NULL;
    ev->read_pt = s5_p1_req;
    event_opt(ev, cdown->fd, EV_R);
    return ev->read_pt(ev);
#else
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
#endif
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


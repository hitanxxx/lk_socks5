#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"

#include "http_req.h"
#include "http_payload.h"
#include "webser.h"

listen_t * g_listens = NULL;
static int g_listenn = 0;

static int listen_add(unsigned short port, event_pt handler, char fssl)
{
    listen_t * nl = sys_alloc(sizeof(listen_t));
    schk(nl != NULL, return -1);
    nl->lport = port;
    nl->fssl = fssl;
    nl->handler = handler;

    if(!g_listens) {
        g_listens = nl;
    } else {
        listen_t * p = g_listens;
        while(p->next) {
            p = p->next;
        }
        p->next = nl;
    }
    g_listenn++;
    return 0;
}

static int listen_open(listen_t * lev)
{
    ///tcp listen
    lev->server_addr.sin_family = AF_INET;
    lev->server_addr.sin_port = htons(lev->lport);
    lev->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    do {
        lev->fd = socket(AF_INET, SOCK_STREAM, 0);
        schk(lev->fd != -1, break);
        schk(net_socket_nbio(lev->fd) == 0, break);
        schk(net_socket_reuseaddr(lev->fd) == 0, break);
        /*
            set reuseport flag, socket listen by all process.
            kernel will be process thundering herd 
        */
        schk(net_socket_reuseport(lev->fd) == 0, break);
        schk(net_socket_fastopen(lev->fd) == 0, break);
        schk(net_socket_nodelay(lev->fd) == 0, break);
        schk(bind(lev->fd, (struct sockaddr *)&lev->server_addr, sizeof(struct sockaddr)) == 0, break);
        schk(listen(lev->fd, 10) == 0, break);
        return 0;
    } while(0);
    return -1;
}

int listen_start(void)
{
    int ret = 0;
    listen_t * p = g_listens;
    while(p) {
        if(0 != listen_open(p)) {
            err("listen open failed\n");
            ret = -1;
            break;
        }
        p = p->next;
    }

    if(ret != 0) {
        p = g_listens;
        while(p) {
            if(p->fd > 0) {
                close(p->fd);
            }
        }
        return ERROR;
    }
    return OK;
}

int listen_init(void)
{    
    int i = 0;

    if(config_get()->s5_mode == TLS_TUNNEL_C) {
        listen_add(config_get()->s5_local_port, tls_tunnel_c_accept, S5_NOSSL); 
    } else if (config_get()->s5_mode == TLS_TUNNEL_S) { 
        listen_add(config_get()->s5_serv_port, tls_tunnel_s_accept, S5_SSL); 
    }
    for(i = 0; i < config_get()->http_num; i++) {
        listen_add(config_get()->http_arr[i], webser_accept_cb, S5_NOSSL);
    }
    for(i = 0; i < config_get()->https_num; i++) {
        listen_add(config_get()->https_arr[i], webser_accept_cb_ssl, S5_SSL);
    }
    if(0 != listen_start()) {
        err("listen start failed\n");
        return -1;
    }
    return 0;
}

status listen_end( void )
{
    if( g_listens ) {
        listen_t * p = g_listens;
        listen_t * n = NULL;
        while(p) {
            n = p->next;
            sys_free(p);
            p = n;
        }
    }
    return OK;
}

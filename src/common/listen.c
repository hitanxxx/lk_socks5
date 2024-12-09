#include "common.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"
#include "http_req.h"
#include "webser.h"

listen_t g_listens[8];

static int listen_add(unsigned short port, ev_cb cb, char fssl)
{
	int i = 0;
	for(i = 0; i < 8; i++) {
		if(!g_listens[i].fuse) {
			g_listens[i].port = port;
			g_listens[i].cb = cb;
			g_listens[i].fssl = fssl ? 1 : 0;

			g_listens[i].fuse = 1;
			return 0;
		}
	}
	return -1;
}

static int listen_set_opt(listen_t * l)
{
	struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(l->port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    do {
        l->fd = socket(AF_INET, SOCK_STREAM, 0);
        schk(l->fd != -1, break);
        schk(net_socket_nbio(l->fd) == 0, break);
        schk(net_socket_reuseaddr(l->fd) == 0, break);
        /*
            set reuseport flag, socket listen by all process.
            kernel will be process thundering herd 
        */
        schk(net_socket_reuseport(l->fd) == 0, break);
        schk(net_socket_fastopen(l->fd) == 0, break);
        schk(net_socket_nodelay(l->fd) == 0, break);
        schk(bind(l->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == 0, break);
        schk(listen(l->fd, 10) == 0, break);
        return 0;
    } while(0);
    return -1;
}

int listen_start(void)
{
	int i = 0;
	for(i = 0; i < 8; i++) {
		if(g_listens[i].fuse) {
			schk(0 == listen_set_opt(&g_listens[i]), return -1);
		}
	}
	return 0;
}

int listen_init(void)
{    
    int i = 0;
	
    if(config_get()->s5_mode == TLS_TUNNEL_C)
        listen_add(config_get()->s5_local_port, tls_tunnel_c_accept, S5_NOSSL); 
    else if (config_get()->s5_mode == TLS_TUNNEL_S)
        listen_add(config_get()->s5_serv_port, tls_tunnel_s_accept, S5_SSL); 
    
	
    for(i = 0; i < config_get()->http_num; i++)
        listen_add(config_get()->http_arr[i], webser_accept_cb, S5_NOSSL);
    
    for(i = 0; i < config_get()->https_num; i++)
        listen_add(config_get()->https_arr[i], webser_accept_cb_ssl, S5_SSL);

	schk(0 == listen_start(), return -1);
    return 0;
}

status listen_end(void)
{
	int i = 0;
	for(i = 0; i < 8; i++) {
		if(g_listens[i].fuse) {
			if(g_listens[i].fd) close(g_listens[i].fd);
		}
	}
	return 0;
}

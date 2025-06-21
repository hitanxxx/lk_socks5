#ifndef _NET_H_INCLUDED_
#define _NET_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

//#define MAX_NET_CON    FD_SETSIZE
#define MAX_NET_CON  768

#define NET_TMOUT 5

typedef int (*net_cb_rw_chain) (con_t * c, meta_t * meta);
typedef int (*net_cb_rw) (con_t * c, unsigned char * buf, int bufn);
typedef void (*con_data_cb) (void * data);

struct net_connection_t {
    struct sockaddr_in addr;    ///listen storget cli addr. connect set server addr
    int fd;
    meta_t * meta;
    ev_t * ev;
    
    void *       data;
    con_data_cb data_cb;
        
    net_cb_rw_chain      send_chain;
    net_cb_rw            send;
    net_cb_rw            recv;

    ssl_con_t* ssl;
    char fssl:1;
};


int net_socket_nbio(int fd);
int net_socket_reuseport(int fd);
int net_socket_reuseaddr(int fd);
int net_socket_fastopen(int fd);
int net_socket_nodelay(  int fd);
int net_socket_nopush(int fd);
int net_socket_lowat_send(int fd);
int net_socket_check_status(int fd);
int net_check_ssl_valid(con_t * c);

int net_accept(con_t * c);
int net_connect(con_t * c, struct sockaddr_in * addr);

int net_alloc(con_t ** c);
int net_free(con_t * c);

int net_init(void);
int net_end(void);
    
#ifdef __cplusplus
}
#endif
    
#endif

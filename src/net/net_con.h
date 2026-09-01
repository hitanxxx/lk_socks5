#ifndef _NET_CON_H_INCLUDED_
#define _NET_CON_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#define NET_EV_MAX      768
#define NET_TMOUT       5000

#define EV_NONE     0x0
#define EV_R        0x1
#define EV_W        0x2

#define L_SSL_CLIENT 0x01
#define L_SSL_SERVER 0x02

typedef struct ev_t ev_t;
typedef struct ev_timer_t ev_timer_t;
typedef struct net_ssl_t net_ssl_t;

/// Callback for raw I/O data processing. 
typedef int (*net_io_cb)(con_t *c, uint8_t *buf, uint32_t bufn);

/// Callback for chained/metadata I/O processing.
typedef int (*net_io_chain_cb)(con_t *c, meta_t *meta);

/// Callback for socket I/O readiness events (EPOLLIN / EPOLLOUT).
typedef int (*net_ev_cb)(con_t *c);

/// Callback for timer expiration.
/// Invoked when the associated timer reaches its timeout value.
typedef void (*net_timer_cb)(ev_timer_t *timer);

/// Cleanup hook for user-defined attached data.
/// Invoked automatically during connection destruction to release the 
/// resources held by the `user_data` pointer.
typedef void (*net_userdata_free_cb)(void *data);


struct net_connection_t {
    struct sockaddr_in addr; /// listen: cli addr. connect: server addr
    int fd;
    meta_t *meta;
    ev_t *ev;
    ev_timer_t *timer;

    void *user_data;
    net_userdata_free_cb free_user_data;

    net_io_cb recv;
    net_io_cb send;
    net_io_chain_cb send_chain;

    net_ev_cb read_cb;
    net_ev_cb write_cb;
    
    net_ssl_t *ssl;
    
    uint8_t fssl : 1;
    uint8_t fclosing : 1;
};

int net_init(void);
int net_exit(void);


int net_connect_chk(con_t *c);
int net_connect(con_t *c, struct sockaddr_in *addr, uint8_t ftcp);
int net_listen(net_ev_cb cb, struct sockaddr_in *addr, uint8_t fssl);

void net_free_timeout(ev_timer_t *timer);
void net_free_thorough(con_t *c);
int net_free(con_t *c);
int net_alloc(con_t **c);


uint32_t net_ev_add(con_t *c, uint32_t mask);
uint32_t net_ev_clr(con_t *c, uint32_t mask);
void net_ev_set(con_t *c, uint32_t new_mask);
void net_ev_loop(void);


int net_timer_add(con_t *c, net_timer_cb cb, uint64_t delay_ms);
int net_timer_del(con_t *c);


ev_timer_t *ev_timer_alloc(net_timer_cb cb, void *data, uint64_t delay_ms);
void ev_timer_free(ev_timer_t *timer);
void *ev_timer_userdata(ev_timer_t *timer);


int net_ssl_create(con_t *c, int flag);
int net_ssl_shutdown(con_t *c);
int net_ssl_handshake(con_t *c);
int net_ssl_check_handshaked(con_t *c);
int net_ssl_check_err(con_t *c);



#ifdef __cplusplus
}
#endif

#endif


#ifndef _TLS_TUNNEL_S_H_INCLUDED_
#define _TLS_TUNNEL_S_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#define TLS_TUNNEL_TMOUT 12 * 1000
#define TLS_TUNNEL_METAN (4096 * 3)

/// @brief MG1(1byte) + MG2(1byte) + DATA LEN(1byte) + DATA
#define TLS_AUTH_MG1 0xae
#define TLS_AUTH_MG2 0x86

typedef struct {
    uint8_t typ;   /// work mode: (c:1)/(s:2)/(s screct:3)
    ///auth data
    uint8_t     auth_data_all;
    uint8_t     auth_data_recv;
    uint8_t     auth_data[32];
    uint8_t     auth_state; /// auth state
    
    ///tunnel use protocol
    int atyp;
    void *adata;

    con_t *cdown;
    con_t *cup;

    dnsc_t *dns;

    uint8_t frecv_err_down : 1;
    uint8_t frecv_err_up : 1;
} tls_tunnel_session_t;

int tls_session_alloc(tls_tunnel_session_t **session);
void tls_session_timeout_release(void *data);
void tls_session_release_by_cdown(void *data);
void tls_session_release_by_cup(void *data);

int tls_tunnel_traffic_proc(con_t *c);
int tls_tunnel_s_start(con_t *c);

int tls_tunnel_s_accept(con_t *c);

int tls_tunnel_s_init(void);
int tls_tunnel_s_exit(void);

#ifdef __cplusplus
}
#endif

#endif

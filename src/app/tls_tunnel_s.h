#ifndef _TLS_TUNNEL_S_H_INCLUDED_
#define _TLS_TUNNEL_S_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define TLS_TUNNEL_TMOUT	 8
#define TLS_TUNNEL_AUTH_MAGIC_NUM     0xa001beef
#define TLS_TUNNEL_METAN    (4096*3)


/// @brief struct of private authorization. 16 Byte
typedef struct {
    uint32_t magic; 
    char  key[16];
    char  secret[16];
} __attribute__((packed)) tls_tunnel_auth_t;


typedef struct {
    char typ;   //tls tunnel work mode: (c)/(s)/(s screct)
    char frecv_err_down:1;
	char frecv_err_up:1;
    con_t * cdown;
    con_t * cup;
    
    dns_cycle_t * dns_ses;

    int atyp;
    void * adata;
} tls_tunnel_session_t;


int tls_ses_free(tls_tunnel_session_t * s5);
int tls_ses_alloc(tls_tunnel_session_t ** s5);
void tls_session_timeout(void * data);


int tls_tunnel_traffic_proc(event_t * ev);
int tls_tunnel_s_transport(event_t * ev);
int tls_tunnel_s_accept(event_t * ev);


int tls_tunnel_s_init(void);
int tls_tunnel_s_exit(void);


#ifdef __cplusplus
}
#endif
    
#endif

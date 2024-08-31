#ifndef _TLS_TUNNEL_S_H_INCLUDED_
#define _TLS_TUNNEL_S_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define TLS_TUNNEL_TMOUT	 8
#define TLS_TUNNEL_AUTH_MAGIC_NUM     0xa001beef
#define TLS_TUNNEL_METAN    8192

/// s5 type for rfc reqest
#define S5_RFC_IPV4         0x1
#define S5_RFC_IPV6         0x4
#define S5_RFC_DOMAIN       0x3

/// @brief struct of private authorization. 16 Byte
typedef struct {
    uint32_t magic; 
    char  key[16];
    char  secret[16];
} __attribute__((packed)) tls_tunnel_auth_t;

typedef struct {
    unsigned char       ver;
    unsigned char       methods_n;
    unsigned char       methods_cnt;
    unsigned char       methods[255];
} s5_ph1_req_t;

typedef struct {
    unsigned char       ver;
    unsigned char       method;
} __attribute__((packed)) s5_ph1_rsp_t;

typedef struct {
    unsigned char       ver;
    unsigned char       cmd;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned char       dst_addr_n;
    unsigned char       dst_addr_cnt;
    unsigned char       dst_addr[DOMAIN_LENGTH];
    unsigned char       dst_port[2];
} s5_ph2_req_t;

typedef struct {
    unsigned char       ver;
    unsigned char       rep;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned int        bnd_addr;
    unsigned short      bnd_port;
} __attribute__((packed)) s5_ph2_rsp_t;

typedef struct {
    char typ;   //tls tunnel work mode: (c)/(s)/(s screct)    
    char frecv_err_down:1;
	char frecv_err_up:1;
    con_t * cdown;
    con_t * cup;

    int s5_state;
    s5_ph1_req_t s5p1;
    s5_ph2_req_t s5p2;
    dns_cycle_t * dns_ses;
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

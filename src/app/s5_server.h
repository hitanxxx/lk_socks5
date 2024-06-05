#ifndef _S5_SERVER_H_INCLUDED_
#define _S5_SERVER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define S5_TIMEOUT	 8
#define S5_AUTH_LOCAL_MAGIC     0xa001beef

/// s5 type for rfc reqest
#define S5_RFC_IPV4         0x1
#define S5_RFC_IPV6         0x4
#define S5_RFC_DOMAIN       0x3

/// @brief struct of private authorization. 16 Byte
typedef struct __attribute__((packed)) s5_auth
{
    uint32_t   magic; 
    char       key[16];
} s5_auth_t ;


typedef struct s5_rfc_phase1_req_s
{
    unsigned char       ver;
    unsigned char       methods_n;
    unsigned char       methods_cnt;
    unsigned char       methods[255];
} s5_rfc_phase1_req_t ;

typedef struct __attribute__((packed)) s5_rfc_phase1_resp_s
{
    unsigned char       ver;
    unsigned char       method;
} s5_rfc_phase1_resp_t ;

typedef struct s5_rfc_phase2_req_s
{
    unsigned char       ver;
    unsigned char       cmd;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned char       dst_addr_n;
    unsigned char       dst_addr_cnt;
    unsigned char       dst_addr[DOMAIN_LENGTH];
    unsigned char       dst_port[2];
} s5_rfc_phase2_req_t ;

typedef struct __attribute__((packed)) s5_rfc_phase2_resp_s
{
    unsigned char       ver;
    unsigned char       rep;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned int        bnd_addr;
    unsigned short      bnd_port;
} s5_rfc_phase2_resp_t ;



typedef struct 
{
    int state;
    char typ;   // client, server, server screct
    
    s5_rfc_phase1_req_t phase1;
    s5_rfc_phase2_req_t phase2;

    con_t * down;
    con_t * up;
    dns_cycle_t * dns_session;

#ifndef S5_OVER_TLS
    sys_cipher_t * cipher_enc;  /// cipher ctx for encrypt data
    sys_cipher_t * cipher_dec;  /// cipher ctx for decrypt data
#endif
    
	char frecv_err_down:1;
	char frecv_err_up:1;
} s5_session_t;


int s5_free(s5_session_t * s5);
int s5_alloc(s5_session_t ** s5);
void s5_timeout_cb(void * data);


int s5_traffic_process(event_t * ev);
int s5_server_transport(event_t * ev);
int s5_server_accept_cb(event_t * ev);

int socks5_server_init(void);
int socks5_server_end(void);


#ifdef __cplusplus
}
#endif
    
#endif

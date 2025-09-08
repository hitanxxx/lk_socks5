#ifndef _TLS_TUNNEL_S_H_INCLUDED_
#define _TLS_TUNNEL_S_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define TLS_TUNNEL_TMOUT    12*1000
#define TLS_TUNNEL_METAN    (4096*3)


/// @brief MG1 + MG2 + 1BYTE DATA LEN + DATA
#define TLS_AUTH_MG1    0xae
#define TLS_AUTH_MG2    0x86

typedef struct {
    char typ;   /// work mode: (c:1)/(s:2)/(s screct:3)
    char state; /// auth state 
    char frecv_err_down:1;
    char frecv_err_up:1;
    con_t * cdown;
    con_t * cup;
    
    dnsc_t * dns;

    char *auth_data;
    char auth_datan;
    char auth_recvd;
    int atyp;
    void * adata;
} tls_tunnel_session_t;


int tls_ses_alloc(tls_tunnel_session_t ** ses);
void tls_ses_exp(void * data);
void tls_ses_release_cdown(void * data);
void tls_ses_release_cup(void * data);


int tls_tunnel_traffic_proc(con_t * c);
int tls_tunnel_s_start(con_t * c);

int tls_tunnel_s_accept(con_t * c);


int tls_tunnel_s_init(void);
int tls_tunnel_s_exit(void);


#ifdef __cplusplus
}
#endif
    
#endif

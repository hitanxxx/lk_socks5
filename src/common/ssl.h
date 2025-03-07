#ifndef _SSL_H_INCLUDED_
#define _SSL_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define SSL_TMOUT        5000

#define L_SSL_CLIENT        0x01
#define L_SSL_SERVER        0x02


typedef struct {
    SSL_CTX    * session_ctx;
    SSL* con;
    
    void * data;
    int cc_ev_typ;
    ev_cb cc_ev_cbr;
    ev_cb cc_ev_cbw;
    
    char    f_handshaked:1;
    char    f_err:1;
    char     f_closed:1;
} ssl_con_t;

int ssl_init(void);
int ssl_end(void);

int ssl_client_ctx(SSL_CTX ** s);
int ssl_server_ctx(SSL_CTX ** s);

int ssl_handshake(con_t * c);
int ssl_shutdown(con_t * c);

int ssl_read(con_t * c, unsigned char * buf, int bufn);
int ssl_write(con_t * c, unsigned char * data, int datan);
int ssl_write_chain(con_t * c, meta_t * meta);

int ssl_create_connection(con_t * c, int flag);
int ssl_conf_set_crt(string_t * value);
int ssl_conf_set_key(string_t * value);

int ssl_load_ctx_certificate(SSL_CTX ** ctx, int flag);
int ssl_load_con_certificate(SSL_CTX * ctx, int flag, SSL ** ssl);

#ifdef __cplusplus
}
#endif
        
#endif

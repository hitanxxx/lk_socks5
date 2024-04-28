#ifndef _SSL_H_INCLUDED_
#define _SSL_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define L_SSL_TIMEOUT		5

#define L_SSL_CLIENT        0x01
#define L_SSL_SERVER        0x02

typedef struct ssl_con_t ssl_con_t;
typedef status ( * ssl_handshake_pt )( event_t * ev );
struct ssl_con_t {
    SSL_CTX	*           session_ctx;
    SSL*                con;
    
    ssl_handshake_pt    cb;
    void *              data;
    int   			    cache_ev_type;
    event_pt            cache_ev_handler;

	char    handshaked:1;
};

status ssl_init( void );
status ssl_end( void );

status ssl_client_ctx( SSL_CTX ** s );
status ssl_server_ctx( SSL_CTX ** s );

status ssl_handshake( ssl_con_t * ssl );
status ssl_shutdown( ssl_con_t * ssl );

ssize_t ssl_read( con_t * c, unsigned char * start, uint32 len );
ssize_t ssl_write( con_t * c, unsigned char * start, uint32 len );
status ssl_write_chain( con_t * c, meta_t * meta );

status ssl_create_connection( con_t * c, uint32 flag );
status ssl_conf_set_crt ( string_t * value );
status ssl_conf_set_key ( string_t * value );

status ssl_load_ctx_certificate( SSL_CTX ** ctx, int flag );
status ssl_load_con_certificate( SSL_CTX * ctx, int flag, SSL ** ssl );

void ssl_record_error( int sslerr );

#ifdef __cplusplus
}
#endif
        
#endif

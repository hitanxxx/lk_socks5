#ifndef _L_SSL_H_INCLUDED_
#define _L_SSL_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define L_SSL_CLIENT        0x01
#define L_SSL_SERVER        0x02

typedef struct l_ssl_connection_t ssl_connection_t;
typedef status ( * ssl_handshake_pt )( event_t * ev );
typedef struct l_ssl_connection_t 
{
    SSL_CTX	*           session_ctx;
    SSL*                con;
    
    ssl_handshake_pt    cb;
    void *              data;
    net_event_type      cache_ev_type;
    event_pt            cache_ev_handler;

	unsigned char       handshaked;
} l_ssl_connection_t;

status ssl_init( void );
status ssl_end( void );

status ssl_client_ctx( SSL_CTX ** s );
status ssl_server_ctx( SSL_CTX ** s );

status ssl_handshake( ssl_connection_t * ssl );
status ssl_shutdown( ssl_connection_t * ssl );

ssize_t ssl_read( connection_t * c, unsigned char * start, uint32 len );
ssize_t ssl_write( connection_t * c, unsigned char * start, uint32 len );
status ssl_write_chain( connection_t * c, meta_t * meta );

status ssl_create_connection( connection_t * c, uint32 flag );
status ssl_conf_set_crt ( string_t * value );
status ssl_conf_set_key ( string_t * value );
    
#ifdef __cplusplus
}
#endif
        
#endif

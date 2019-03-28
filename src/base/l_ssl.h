#ifndef _L_SSL_H_INCLUDED_
#define _L_SSL_H_INCLUDED_


#define 	L_SSL_CLIENT			10000
#define 	L_SSL_SERVER			10001

#define 	L_SSL_CERT_PATH_LEN		128
#define 	L_SSL_KEY_PATH_LEN		128

// ssl
typedef struct ssl_connection_t ssl_connection_t;
typedef status ( * ssl_handshake_pt )( event_t * ev );
typedef struct ssl_connection_t {
	SSL_CTX	*			session_ctx;
	SSL* 				con;

	ssl_handshake_pt 	handler;
	void * 				data;

	uint32				handshaked;

	event_pt			old_read_handler;
	event_pt			old_write_handler;

} ssl_connection_t;

//
status ssl_init( void );
status ssl_end( void );

status ssl_client_ctx( SSL_CTX ** s );
status ssl_server_ctx( SSL_CTX ** s );

status ssl_handshake( ssl_connection_t * ssl );

status ssl_shutdown( ssl_connection_t * ssl );
status ssl_shutdown_handler( event_t * ev );

ssize_t ssl_read( connection_t * c, char * start, uint32 len );
ssize_t ssl_write( connection_t * c, char * start, uint32 len );
status ssl_write_chain( connection_t * c, meta_t * meta );

status ssl_create_connection( connection_t * c, uint32 flag );


status ssl_conf_set_crt ( string_t * value );
status ssl_conf_set_key ( string_t * value );
#endif

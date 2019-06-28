#ifndef _L_NET_H_INCLUDED_
#define _L_NET_H_INCLUDED_

#define MAXCON	2048

typedef status ( *net_send_chain ) ( connection_t * c, meta_t * meta );
typedef status ( *net_recv_chain ) ( connection_t * c, meta_t * meta );
typedef ssize_t ( *net_send ) ( connection_t * c, char * start, uint32 len );
typedef ssize_t ( *net_recv ) ( connection_t * c, char * start, uint32 len );

typedef struct l_connection_t {
	queue_t				queue;

	int32				fd;
	void*				data;
	meta_t*				meta;

	struct sockaddr_in 	addr;
	int32				active_flag;

	ssl_connection_t*	ssl;
	uint32				ssl_flag;

	event_t				event;
	// this timer is optional to use
	l_timer_t*			timer;

	net_send_chain 		send_chain;
	net_recv_chain		recv_chain;
	net_send 			send;
	net_recv 			recv;
} l_connection_t;

struct addrinfo * net_get_addr( string_t * ip, string_t * port );

status l_socket_nonblocking( int32 fd );
status l_socket_reuseaddr( int32 fd );
status l_socket_fastopen( int32 fd );
status l_socket_nodelay(  int32 fd );
status l_socket_nopush( int32 fd );
status l_socket_check_status( int fd );


status net_alloc( connection_t ** connection );
status net_free( connection_t * connection );

status net_init( void );
status net_end( void );


#endif

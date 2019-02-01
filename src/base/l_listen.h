#ifndef _L_LISTEN_H_INCLUDED_
#define _L_LISTEN_H_INCLUDED_

#define TCP		 0x0009
#define HTTP	 0x1000
#define HTTPS	 0x1001
#define LKTP	 0x1002

extern mem_list_t * listens;
//listen
typedef status ( *listen_pt ) ( event_t * ev );
typedef struct listen_t {
	queue_t				queue;
	uint32 				port;
	uint32				type;
	listen_pt			handler;

	int32				fd;
	struct sockaddr_in  server_addr;
	connection_t 		* c;
	mem_list_t			*list;

	uint32				error;
	uint32 				reuse_port;
} listen_t;

status listen_add( uint32 port, listen_pt handler, uint32 type );
status listen_start( void );
status listen_stop( void );

status listen_init( void );
status listen_end( void );

#endif

#ifndef _L_LISTEN_H_INCLUDED_
#define _L_LISTEN_H_INCLUDED_

#define L_SSL		0x01
#define L_NOSSL		0x02

typedef status ( *listen_pt ) ( event_t * event );
typedef struct listen_t {
	queue_t				queue;
	int32				fd;
	
	uint32 				port;
	uint32				type;

	listen_pt			handler;

	event_t				event;
	struct sockaddr_in  server_addr;
} listen_t;

status listen_add( uint32 port, listen_pt handler, uint32 type );
status listen_start( void );
status listen_stop( void );

status listen_init( void );
status listen_end( void );

#endif

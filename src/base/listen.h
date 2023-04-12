#ifndef _LISTEN_H_INCLUDED_
#define _LISTEN_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define L_SSL		0x01
#define L_NOSSL		0x02

extern mem_arr_t * listens;

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

status listen_start( void );
status listen_stop( void );

status listen_init( void );
status listen_end( void );

int listen_num( );


#ifdef __cplusplus
}
#endif
    
#endif
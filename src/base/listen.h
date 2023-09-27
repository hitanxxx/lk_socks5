#ifndef _LISTEN_H_INCLUDED_
#define _LISTEN_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    


extern mem_arr_t * listens;

typedef status ( *listen_pt ) ( event_t * event );
typedef struct listen_t {
	queue_t				queue;
	int32				fd;
	
	unsigned short 				port;
	unsigned char				type;

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

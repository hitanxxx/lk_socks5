#ifndef _LISTEN_H_INCLUDED_
#define _LISTEN_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct listen listen_t;

extern listen_t * g_listens;

struct listen {
    int fd;
	char fssl:1;
	unsigned short lport;
	
	event_pt handler;
	event_t	event;
	struct sockaddr_in server_addr;

    listen_t * next;
};

status listen_start( void );
status listen_init( void );
status listen_end( void );


#ifdef __cplusplus
}
#endif
    
#endif

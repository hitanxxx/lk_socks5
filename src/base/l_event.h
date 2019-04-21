#ifndef _L_EVENT_H_INCLUDED_
#define _L_EVENT_H_INCLUDED_

#define EV_MAXEVENTS 2048

#define EV_NONE       (0x00)
#define EV_R          EPOLLIN
#define EV_W          EPOLLOUT

typedef unsigned short  net_events;
typedef struct event_t event_t;
typedef status ( *event_pt ) ( event_t * event );
typedef struct event_t {

	event_pt		read_pt;
	event_pt 		write_pt;
	void *			data;

	// every event should have a timer to control network time out
	l_timer_t		timer;
	
	uint32			f_active;
	uint32			f_read;
	uint32			f_write;
}event_t;

status l_net_accept( event_t * event );
status l_net_connect(connection_t * c, string_t* ip, string_t *port );

status event_opt( event_t * event, int32 fd, net_events events );
status event_run( time_t time_out );

status event_process_init( void );
status event_process_end( void );

status event_init( void );
status event_end( void );

#endif

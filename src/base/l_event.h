#ifndef _L_EVENT_H_INCLUDED_
#define _L_EVENT_H_INCLUDED_

#define MAXEVENTS 2048

#define EVENT_NONE           (0x00)
#define EVENT_READ           EPOLLIN
#define EVENT_WRITE          EPOLLOUT
#define EVENT_RW			 EVENT_READ|EVENT_WRITE

extern queue_t		queue_event;
extern sem_t*		accept_mutex;
extern pid_t		accept_mutex_user;

// ----
typedef unsigned short  net_events;
typedef struct event_t event_t;

typedef status ( *event_pt ) ( event_t * event );
typedef struct event_t {
	queue_t			queue;
	event_pt		handler;
	void *			data;
	timer_msg_t 	timer;

	uint32			f_active;
	uint32			f_accept;
}event_t;

status event_accept( event_t * event );
status event_connect( event_t * event );

status event_close( event_t * event, uint32 flag );
status event_opt( event_t * event, net_events flag );
status event_loop( time_t time_out );

status event_process_init( void );
status event_process_end( void );

status event_init( void );
status event_end( void );

#endif

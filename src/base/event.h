#ifndef _EVENT_H_INCLUDED_
#define _EVENT_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

/// event options 
#define EV_NONE         0x0
#if defined(EVENT_EPOLL)
#define EV_R            EPOLLIN
#define EV_W            EPOLLOUT
#else
#define EV_R            0x001
#define EV_W            0x004
#endif

typedef struct l_event event_t;
typedef status ( *event_pt ) ( event_t * event );
struct l_event 
{
    queue_t         queue;
    int32           fd;
    // every event should have a timer to control network time out
    ev_timer_t      timer;
	void *          data;
    /// cache previously event option (EV_R,EV_W,EV_NONE)
	int 			opt;

    /// listen event yes or not 
    char            f_listen;
    /// event can disable by other events 
	char   			f_active;  

    /// mark readable, writable
    char    f_read;
    char    f_write;

    event_pt        read_pt;
    event_pt        write_pt;
};
typedef status (*event_handler_init) (void);
typedef status (*event_handler_end) (void);
typedef status (*event_handler_opt)( event_t * ev, int32 fd, int type );
typedef status (*event_handler_run)( time_t msec );
typedef struct event_handler {
    event_handler_init      init;
    event_handler_end       end;
    event_handler_opt       opt;
    event_handler_run       run;
} event_handler_t;

status event_opt( event_t * event, int32 fd, int type );
status event_run( time_t msec );

status event_alloc( event_t ** ev );
status event_free( event_t * ev );

status event_init( void );
status event_end( void );

status event_post_event( event_t * ev );


#ifdef __cplusplus
}
#endif
        
    
#endif

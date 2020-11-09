#ifndef _L_EVENT_H_INCLUDED_
#define _L_EVENT_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define EV_NONE         0x0
#if defined(EVENT_EPOLL)
#define EV_R            EPOLLIN
#define EV_W            EPOLLOUT
#else
#define EV_R            0x1
#define EV_W            0x2
#endif

typedef unsigned short  net_event_type;
typedef struct l_event event_t;
typedef status ( *event_pt ) ( event_t * event );
struct l_event 
{
    queue_t         queue;
    int32           fd;
    int32           idx;
    net_event_type  type;
    
    event_pt        read_pt;
    event_pt        write_pt;
    void *          data;

    // every event should have a timer to control network time out
    l_timer_t       timer;
    unsigned char   f_active;
    unsigned char   f_read;
    unsigned char   f_write;
    unsigned char   f_position;
};
typedef status (*event_handler_init) (void);
typedef status (*event_handler_end) (void);
typedef status (*event_handler_opt)( event_t * ev, int32 fd, net_event_type type );
typedef status (*event_handler_run)( time_t msec );
typedef struct event_handler
{
    event_handler_init      init;
    event_handler_end       end;
    event_handler_opt       opt;
    event_handler_run       run;
} event_handler_t;

status event_opt( event_t * event, int32 fd, net_event_type type );
status event_run( time_t msec );

status event_alloc( event_t ** ev );
status event_free( event_t * ev );

status event_init( void );
status event_end( void );

#ifdef __cplusplus
}
#endif
        
    
#endif

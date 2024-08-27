#ifndef _EVENT_H_INCLUDED_
#define _EVENT_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define EV_NONE  0x0
#if defined(EVENT_EPOLL)
#define EV_R   EPOLLIN
#define EV_W   EPOLLOUT
#else
#define EV_R   0x001
#define EV_W   0x004
#endif

typedef struct event event_t;
typedef status (*event_pt) (event_t * event);
struct event {
    
#ifndef EVENT_EPOLL
    queue_t     queue;
#endif

    int fd;
    ev_timer_t timer;  /// every event should have a timer to control network time out
	void * data;
	int opt;    /// cache previously event option (EV_R,EV_W,EV_NONE)

    int idxr;
    int idxw;

    event_pt  read_pt;
    event_pt  write_pt;

    char    flisten:1; /// listen event yes or not 
    char    fread:1;   /// mark readable, writable
    char    fwrite:1;
};
typedef int (*event_handler_init) (void);
typedef int (*event_handler_end) (void);
typedef int (*event_handler_opt)(event_t * ev, int fd, int type);
typedef int (*event_handler_run)(time_t msec);
typedef struct event_handler {
    event_handler_init      init;
    event_handler_end       end;
    event_handler_opt       opt;
    event_handler_run       run;
} event_handler_t;

int event_opt(event_t * event, int fd, int type);
int event_run(time_t msec);

int event_alloc(event_t ** ev);
int event_free(event_t * ev);

int event_init(void);
int event_end(void);

int event_post_event(event_t * ev);

#ifdef __cplusplus
}
#endif
        
    
#endif

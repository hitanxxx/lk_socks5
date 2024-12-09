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

typedef struct ev ev_t;
struct ev {
    queue_t  queue;
    con_t * c;
    ev_timer_t timer;  /// every event should have a timer to control network time out
    int opt;    /// cache previously event option (EV_R,EV_W,EV_NONE)

    int idxr;
    int idxw;

    ev_cb  read_cb;
    ev_cb  write_cb;
    char  fread:1;   /// mark readable, writable
    char  fwrite:1;
};

int ev_opt(con_t * c, int type);
int ev_loop(time_t msec);

int ev_alloc(ev_t ** ev);
int ev_free(ev_t * ev);

int ev_init(void);
int ev_exit(void);


typedef int (*event_handler_init) (void);
typedef int (*event_handler_end) (void);
typedef int (*event_handler_opt)(con_t * c, int type);
typedef int (*event_handler_run)(time_t msec);
typedef struct event_handler {
    event_handler_init        init;
    event_handler_end        end;
    event_handler_opt        opt;
    event_handler_run        run;
} event_handler_t;

#ifdef __cplusplus
}
#endif
        
    
#endif

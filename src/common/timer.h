#ifndef _TIMER_H_INCLUDED_
#define _TIMER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ev_timer {
    heap_node_t    node;
    ev_cb cb;
    char                f_timeset:1;
    char                f_timeout:1;
} ev_timer_t;

int tm_add(con_t * c, void * cb, int delay_ms);
int tm_del(con_t * c);


int timer_expire(int * timer);

int timer_init(void);
int timer_end(void);


#ifdef __cplusplus
}
#endif

#endif

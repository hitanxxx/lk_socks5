#ifndef _NET_EVENT_TIMER_H_INCLUDED_
#define _NET_EVENT_TIMER_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

struct ev_timer_t {
    heap_node_t node;

    uint64_t delay_ms;
    void *user_data;
    net_timer_cb cb;
    
    uint8_t f_timeset : 1;
    uint8_t f_once : 1;
} ;

uint64_t ev_timer_remaining(void);

int ev_timer_init(void);
int ev_timer_exit(void);

int ev_timer_del(ev_timer_t *timer);
int ev_timer_add(ev_timer_t *timer, net_timer_cb cb, void *data, uint64_t delay_ms);


#ifdef __cplusplus
}
#endif

#endif


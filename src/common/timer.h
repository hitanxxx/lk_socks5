#ifndef _TIMER_H_INCLUDED_
#define _TIMER_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*timer_cb)(void *data);
typedef struct ev_timer_s {
    heap_node_t node;

    timer_cb cb;
    void *data;
    
    uint8_t f_timeset : 1;
    uint8_t f_timeout : 1;
    uint8_t reserve : 2;
} ev_timer_t;

int tm_add(ev_timer_t *timer, timer_cb cb, void *data, int delay_ms);
int tm_del(ev_timer_t *timer);

int timer_remaining(uint64_t *wait_ms);
int timer_init(void);
int timer_end(void);

#define EZ_TMADD(x, y, z) tm_add(&((x)->ev->timer), (y), (x), (z))
#define EZ_TMDEL(x) tm_del(&((x)->ev->timer))

#ifdef __cplusplus
}
#endif

#endif

#include "common.h"
#include "net_event.h"
#include "net_event_timer.h"
#include "net_ssl.h"

static heap_t *g_heap = NULL;

static ev_timer_t *ev_timer_minimum(void) {
    heap_node_t *min_node = heap_min(g_heap);
    if (min_node) {
        ev_timer_t *min_timer = ptr_get_struct(min_node, ev_timer_t, node);
        return min_timer;
    }
    return NULL;
}

static int ev_timer_del_internal(ev_timer_t *timer) {
    if (timer->f_timeset) {
        
        schk(heap_del(g_heap, timer->node.index) == 0, return -1);
        timer->f_timeset = 0;
    }
    return 0;
}

int ev_timer_del(ev_timer_t *timer) {
    if (timer->f_timeset) {
        uint8_t fwake = 0;
        if (timer == ev_timer_minimum())
            fwake = 1;
    
        schk(heap_del(g_heap, timer->node.index) == 0, return -1);
        timer->f_timeset = 0;

        if (fwake) {
            ev_wake();
        }
    }
    return 0;
}

int ev_timer_add(ev_timer_t *timer, net_timer_cb cb, void *data, uint64_t delay_ms) {
    if (timer->f_timeset)
        ev_timer_del(timer);

    timer->delay_ms = delay_ms;
    timer->node.key = (systime_msec() + delay_ms);
    timer->cb = cb;
    timer->user_data = data;
    timer->f_timeset = 1;
    schk(0 == heap_add(g_heap, &timer->node), return -1);

    if (timer == ev_timer_minimum()) {
        ev_wake();
    }
    return 0;
}

uint64_t ev_timer_remaining(void) {
    uint64_t timestamp = 0;
    for (;;) {
        ev_timer_t *timer = ev_timer_minimum();
        if (timer) {
            if (timer->node.key > systime_msec()) {
                timestamp = (timer->node.key - systime_msec());
                break;
            } else {
                ev_timer_del_internal(timer);
                if (!timer->f_once) {
                    ev_timer_add(timer, timer->cb, timer->user_data, timer->delay_ms);
                }
                
                if (timer->cb) {
                    timer->cb(timer);
                }
            }
        } else {
            timestamp = -1;
            break;
        }
    }
    return timestamp;
}

int ev_timer_init(void) {
    heap_create(&g_heap, NET_EV_MAX * 2);
    return 0;
}

int ev_timer_exit(void) {
    if (g_heap)
        heap_free(g_heap);
    return 0;
}

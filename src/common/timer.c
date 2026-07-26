#include "common.h"

static heap_t *g_heap = NULL;

int tm_del(ev_timer_t *timer) {
    if (timer->f_timeset) {
        schk(heap_del(g_heap, timer->node.index) == 0, return -1);
        timer->f_timeset = 0;
    }
    return 0;
}

int tm_add(ev_timer_t *timer, timer_cb cb, void *data, int delay_ms) {
    if (timer->f_timeset)
        tm_del(timer);

    timer->node.key = (systime_msec() + delay_ms);
    timer->cb = cb;
    timer->data = data;
    timer->f_timeout = 0;
    timer->f_timeset = 1;
    schk(0 == heap_add(g_heap, &timer->node), return -1);
    return 0;
}

static ev_timer_t *timer_minimum(void) {
    heap_node_t *min_node = heap_min(g_heap);
    if (min_node) {
        ev_timer_t *min_timer = ptr_get_struct(min_node, ev_timer_t, node);
        return min_timer;
    }
    return NULL;
}

int timer_remaining(uint64_t *ms) {

    for (;;) {
        ev_timer_t *timer_node = timer_minimum();
        if (timer_node) {
            if (timer_node->node.key > systime_msec()) {
                *ms = (timer_node->node.key - systime_msec());
                break;
            } else {
                timer_node->f_timeout = 1;
                tm_del(timer_node);
                if (timer_node->cb) {
                    timer_node->cb(timer_node->data);
                }
            }
        } else {
            *ms = 200;
            break;
        }
    }
    return 0;
}

int timer_init(void) {
    heap_create(&g_heap, MAX_NET_CON * 2);
    return 0;
}

int timer_end(void) {
    if (g_heap)
        heap_free(g_heap);
    return 0;
}

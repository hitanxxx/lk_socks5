#include "common.h"

static heap_t *g_heap = NULL;

int tm_del(ev_timer_t *timer) {
    if (timer->f_timeset) {
        timer->f_timeset = 0;
        schk(heap_del(g_heap, timer->node.index) == 0, return -1);
    }
    return 0;
}

int tm_add(ev_timer_t *timer, timer_cb cb, void *data, int delay_ms) {
    if (timer->f_timeset)
        tm_del(timer);

    timer->f_timeout = 0;
    timer->f_timeset = 1;
    timer->cb = cb;
    timer->data = data;
    timer->node.key = (systime_msec() + delay_ms);
    schk(0 == heap_add(g_heap, &timer->node), return -1);
    return 0;
}

static ev_timer_t *timer_min(void) {
    heap_node_t *min_node = heap_min(g_heap);
    if (!min_node)
        return NULL;

    ev_timer_t *min_timer = ptr_get_struct(min_node, ev_timer_t, node);
    return min_timer;
}

int timer_expire(int *wait_ms) {
    ev_timer_t *oldest = NULL;

    for (;;) {

        oldest = timer_min();
        if (!oldest) {
            *wait_ms = 200;
            return 0;
        }

        if (oldest->node.key > systime_msec()) {
            *wait_ms = (int)(oldest->node.key - systime_msec());
            return 0;
        } else {
            oldest->f_timeout = 1;
            tm_del(oldest);
            if(oldest->cb)
                oldest->cb(oldest->data);
        }
    }
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

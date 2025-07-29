#include "common.h"


static heap_t * g_heap = NULL;


int tm_del(con_t * c)
{    
    if(c->ev->timer.f_timeset) {
        schk(heap_del(g_heap, c->ev->timer.node.index) == 0, return -1);
        c->ev->timer.f_timeset = 0;
    }
    return 0;
}

int tm_add(con_t * c, void * cb, int delay_ms)
{
    if(c->ev->timer.f_timeset) 
        tm_del(c);

    c->ev->timer.node.key = (systime_msec() + delay_ms);
    schk(0 == heap_add(g_heap, &c->ev->timer.node), return -1);
    c->ev->timer.cb = cb;
    c->ev->timer.f_timeout = 0;
    c->ev->timer.f_timeset = 1;
    return 0;
}

static ev_timer_t * timer_min(void)
{
    ev_timer_t * min_timer;
    heap_node_t * min_node = NULL;

    min_node = heap_min(g_heap);
    schk(min_node, return NULL);
    min_timer = ptr_get_struct(min_node, ev_timer_t, node);
    return min_timer;
}

int timer_expire(int * timer)
{
    ev_timer_t * oldest = NULL;

    for(;;) {
        if(heap_empty(g_heap)) {
            *timer = 200;
            return 0;
        }
        oldest = timer_min();
        if((oldest->node.key - systime_msec()) > 0)  {
            *timer = (int)(oldest->node.key - systime_msec());
            return 0;
        } else {
            oldest->f_timeout = 1;
            ev_t * ev = ptr_get_struct(oldest, ev_t, timer);
            tm_del(ev->c);
            if(oldest->cb) {
				oldest->cb(ev->c);
			}
        }
    }
}

int timer_init(void)
{
    heap_create(&g_heap, MAX_NET_CON*2);
    return 0;
}

int timer_end(void)
{
    if(g_heap)
        heap_free(g_heap);
    return 0;
}


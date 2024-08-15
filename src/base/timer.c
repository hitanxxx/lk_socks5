#include "common.h"


static heap_t * g_heap = NULL;

int timer_add(ev_timer_t * timer, int sec)
{
    if(timer->f_timeset)
        timer_del(timer);

    timer->node.key = (systime_msec() + (sec * 1000));
    schk(heap_add(g_heap, &timer->node) == 0, return -1);
    timer->f_timeset = 1;
    timer->f_timeout = 0;
    return 0;
}

inline void timer_set_data(ev_timer_t * timer, void * data)
{
    timer->data = data;
}

inline void timer_set_pt(ev_timer_t * timer, timer_pt pt)
{
    timer->timeout_handler = pt;
}


int timer_del(ev_timer_t * timer)
{
    if(!timer->f_timeset)
        return 0;

    schk(heap_del(g_heap, timer->node.index) == 0, return -1);
    timer->f_timeset = 0;
    return 0;
}


static ev_timer_t * timer_min(void)
{
    ev_timer_t * min_timer;
    heap_node_t * min = NULL;

    min = heap_min(g_heap);
    schk(min, return NULL);
    min_timer = ptr_get_struct(min, ev_timer_t, node);
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
            timer_del(oldest);
            oldest->f_timeout = 1;
            if(oldest->timeout_handler)
                oldest->timeout_handler(oldest->data);
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

#if (1)
int timer_free(ev_timer_t * timer)
{
    if(timer) {
        if(timer->f_timeset)
            timer_del(timer);
        
        sys_free(timer);
        timer = NULL;
    }
    return 0;
}

int timer_alloc(ev_timer_t ** timer)
{
    ev_timer_t * ltimer = (ev_timer_t *)sys_alloc(sizeof(ev_timer_t));
    schk(ltimer, return -1);
    *timer = ltimer;
    return 0;
}
#endif

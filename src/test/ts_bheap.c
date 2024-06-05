#include "common.h"
#include "test_main.h"

//static l_timer_t g_ts_timer;
static void ts_bheap_add_timer()
{
    //memset( &g_ts_timer, 0, sizeof(l_timer_t) );
    //timer_set_pt( &g_ts_timer, ts_bheap_timer_pt );
    //timer_add( &g_ts_timer, 3 );
}

void ts_bheap_init()
{
    test_add(ts_bheap_add_timer);
}

#include "lk.h"

static heap_t * heap = NULL;
// timer_add -----------------------------------------
status timer_add( timer_msg_t * timer, uint32 sec )
{
	if( 1 == timer->f_timeset ) {
		timer_del( timer );
	}
	timer->node.num = cache_time_msec + ( sec * 1000 );
	if( OK != heap_add( heap, &timer->node ) ) {
		err_log( "%s --- heap insert", __func__ );
		return ERROR;
	}
	timer->f_timeset = 1;
	return OK;
}
// timer_del ----------------------------------
status timer_del( timer_msg_t * timer )
{
	if( 0 == timer->f_timeset ) {
		return OK;
	}
	if( OK != heap_del( heap, timer->node.index ) ) {
		err_log( "%s --- heap del", __func__ );
		return ERROR;
	}
	timer->f_timeset = 0;
	return OK;
}
// timer_min ------------------------------------
static timer_msg_t * timer_min( void )
{
	timer_msg_t * min_timer;
	heap_node_t * min = NULL;

	min = heap_get_min( heap );
	if( !min ) {
		err_log( "%s --- heap min", __func__ );
		return NULL;
	}
	min_timer = l_get_struct( min, timer_msg_t, node );
	return min_timer;
}
// timer_expire ---------------------
status timer_expire( int32 * timer )
{
	timer_msg_t * oldest = NULL;

	while(1) {
		if( 0 == heap->index ) {
			*timer = -1;
			return OK;
		}
		oldest = timer_min( );
		if( ( oldest->node.num - cache_time_msec ) > 0 ) {
			*timer = (int32)( oldest->node.num - cache_time_msec );
			return OK;
		} else {
			timer_del( oldest );
			if( oldest->handler ) {
				oldest->handler( oldest->data );
			}
		}
	}
}
// timer_init --------------------------------
status timer_init( void )
{
	heap_create( &heap, MAXCON*2 );
	return OK;
}
// timer_end ----------------------------------
status timer_end( void )
{
	if( heap ) {
		heap_free( heap );
	}
	return OK;
}

#include "lk.h"

static heap_t * heap = NULL;

status timer_add( l_timer_t * timer, uint32 sec )
{
	if( 1 == timer->f_timeset ) {
		timer_del( timer );
	}
	timer->node.num = cache_time_msec + ( sec * 1000 );
	if( OK != heap_add( heap, &timer->node ) ) {
		err(" heap insert\n" );
		return ERROR;
	}
	timer->f_timeset = 1;
	return OK;
}

status timer_del( l_timer_t * timer )
{
	if( 0 == timer->f_timeset ) {
		return OK;
	}
	if( OK != heap_del( heap, timer->node.index ) ) {
		err(" heap del\n" );
		return ERROR;
	}
	timer->f_timeset = 0;
	timer->timeout_handler = NULL;
	timer->data = NULL;
	return OK;
}

status timer_free( l_timer_t * timer )
{
	if( timer ) {
		if( timer->f_timeset ) {
			timer_del( timer );
		}
		l_safe_free(timer);
		timer = NULL;
	}
	return OK;
}

status timer_alloc( l_timer_t ** timer )
{
	l_timer_t * new = NULL;

	new = (l_timer_t *)l_safe_malloc( sizeof(l_timer_t) );
	if( !new ) {
		err(" safe malloc timer failed\n" );
		return ERROR;
	}
	memset( new, 0, sizeof(l_timer_t) );
	*timer = new;
	return OK;
}

static l_timer_t * timer_min( void )
{
	l_timer_t * min_timer;
	heap_node_t * min = NULL;

	min = heap_get_min( heap );
	if( !min ) {
		err(" heap min\n" );
		return NULL;
	}
	min_timer = l_get_struct( min, l_timer_t, node );
	return min_timer;
}

status timer_expire( int32 * timer )
{
	l_timer_t * oldest = NULL;

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
			if( oldest->timeout_handler ) {
				oldest->timeout_handler( oldest->data );
			}
		}
	}
}

void timer_set_data( l_timer_t * timer, void * data )
{
	timer->data = data;
}

void timer_set_pt( l_timer_t * timer, timer_pt pt )
{
	timer->timeout_handler = pt;
}

status timer_init( void )
{
	heap_create( &heap, MAXCON*2 );
	return OK;
}

status timer_end( void )
{
	if( heap ) {
		heap_free( heap );
	}
	return OK;
}

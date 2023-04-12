#include "common.h"


static heap_t * g_heap = NULL;

status timer_add( ev_timer_t * timer, uint32 sec )
{
	if( 1 == timer->f_timeset ) 
	{
		timer_del( timer );
	}
	timer->node.key = systime_msec() + ( sec * 1000 );
	if( OK != heap_add( g_heap, &timer->node ) ) 
	{
		err(" heap insert\n" );
		return ERROR;
	}
	timer->f_timeset = 1;
	timer->f_timeout = 0;
    
	return OK;
}

inline void timer_set_data( ev_timer_t * timer, void * data )
{
	timer->data = data;
}

inline void timer_set_pt( ev_timer_t * timer, timer_pt pt )
{
	timer->timeout_handler = pt;
}


status timer_del( ev_timer_t * timer )
{
	if( 0 == timer->f_timeset ) 
	{
		return OK;
	}
	if( OK != heap_del( g_heap, timer->node.index ) ) 
	{
		err(" heap del\n" );
		return ERROR;
	}
	timer->f_timeset = 0;
	return OK;
}


static ev_timer_t * timer_min( void )
{
	ev_timer_t * min_timer;
	heap_node_t * min = NULL;

	min = heap_min( g_heap );
	if( !min ) 
	{
		err("heap min\n" );
		return NULL;
	}
	min_timer = ptr_get_struct( min, ev_timer_t, node );
	return min_timer;
}

status timer_expire( int32 * timer )
{
	ev_timer_t * oldest = NULL;

	while(1) 
	{
		if( OK == heap_empty(g_heap) ) 
		{
			*timer = 200;
			return OK;
		}
		oldest = timer_min( );
		if( ( oldest->node.key - systime_msec() ) > 0 ) 
		{
			*timer = (int32)( oldest->node.key - systime_msec() );
			return OK;
		} 
		else 
		{
			timer_del( oldest );
			oldest->f_timeout = 1;
			if( oldest->timeout_handler ) 
			{
				oldest->timeout_handler( oldest->data );
			}
		}
	}
}



status timer_init( void )
{
    debug("\n");
	heap_create( &g_heap, MAX_NET_CON*2 );
	return OK;
}

status timer_end( void )
{
	if( g_heap ) 
	{
		heap_free( g_heap );
	}
	return OK;
}

#if (1)
status timer_free( ev_timer_t * timer )
{
	if( timer ) 
	{
		if( timer->f_timeset ) 
		{
			timer_del( timer );
		}
		l_safe_free(timer);
		timer = NULL;
	}
	return OK;
}

status timer_alloc( ev_timer_t ** timer )
{
	ev_timer_t * new = NULL;

	new = (ev_timer_t *)l_safe_malloc( sizeof(ev_timer_t) );
	if( !new ) 
	{
		err("safe malloc timer failed\n" );
		return ERROR;
	}
	memset( new, 0, sizeof(ev_timer_t) );
	*timer = new;
	return OK;
}
#endif

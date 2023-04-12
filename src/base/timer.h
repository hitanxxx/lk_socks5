#ifndef _TIMER_H_INCLUDED_
#define _TIMER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef void ( * timer_pt ) ( void * data );
typedef struct ev_timer {
	void * 				data;
	int32				f_timeset;
	int32				f_timeout;

	heap_node_t			node;
	timer_pt 			timeout_handler;
} ev_timer_t;

status timer_add( ev_timer_t * timer, uint32 sec );
void timer_set_data( ev_timer_t * timer, void * data );
void timer_set_pt( ev_timer_t * timer, timer_pt pt );

status timer_del( ev_timer_t * timer );
status timer_expire( int32 * timer );

status timer_init( void );
status timer_end( void );

#if(1)	
status timer_alloc( ev_timer_t ** timer );
status timer_free( ev_timer_t * timer );
#endif

#ifdef __cplusplus
}
#endif

#endif

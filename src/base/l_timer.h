#ifndef _L_TIMER_H_INCLUDED_
#define _L_TIMER_H_INCLUDED_


// timer
typedef void ( * timer_handler ) ( void * data );
typedef struct timer_msg_t {
	void * 				data;
	int32				f_timeset;
	int32				f_timeout;

	heap_node_t			node;
	timer_handler 		handler;
} timer_msg_t;


status timer_init( void );
status timer_end( void );
status timer_add( timer_msg_t * timer, uint32 sec );
status timer_del( timer_msg_t * timer );
status timer_expire( int32 * timer );

#endif

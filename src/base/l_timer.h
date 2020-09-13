#ifndef _L_TIMER_H_INCLUDED_
#define _L_TIMER_H_INCLUDED_

typedef void ( * timer_pt ) ( void * data );
typedef struct l_timer_t {
	void * 				data;
	int32				f_timeset;
	int32				f_timeout;

	heap_node_t			node;
	timer_pt 			timeout_handler;
} l_timer_t;

status timer_add( l_timer_t * timer, uint32 sec );
status timer_del( l_timer_t * timer );

status timer_alloc( l_timer_t ** timer );
status timer_free( l_timer_t * timer );

status timer_expire( int32 * timer );

inline void timer_set_data( l_timer_t * timer, void * data );
inline void timer_set_pt( l_timer_t * timer, timer_pt pt );

status timer_init( void );
status timer_end( void );


#endif

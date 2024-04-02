#include "common.h"
#include "test_main.h"


static queue_t arr[3];
static queue_t head;

// ------------
static void ts_queue_start( void )
{
    queue_init( &head );
}
// --------------
static void ts_queue_add( void )
{
    queue_insert( &head, &arr[0] );
    t_assert( queue_head( &head ) == &arr[0] );
    t_assert( queue_prev( queue_tail( &head ) ) == &arr[0] );
    
    queue_insert( &head, &arr[1] );
    t_assert( queue_head( &head ) == &arr[1] );
    t_assert( queue_prev ( queue_tail( &head ) ) == &arr[0] );
    
    queue_insert( &head, &arr[2] );
    t_assert( queue_head( &head ) == &arr[2] );
    t_assert( queue_prev ( queue_tail( &head ) ) == &arr[0] );
}
// --------------
static void ts_queue_del( void )
{
    queue_remove( &arr[2] );
    t_assert( queue_head( &head ) == &arr[1] );
    t_assert( queue_prev ( queue_tail( &head ) ) == &arr[0] );
    
    queue_remove( &arr[1] );
    t_assert( queue_head( &head ) == &arr[0] );
    t_assert( queue_prev ( queue_tail( &head ) ) == &arr[0] );
    
    queue_remove( &arr[0] );
    t_assert( queue_empty( &head ) );
}
// -------------
void ts_queue_init( )
{
    test_add( ts_queue_start );
    test_add( ts_queue_add );
    test_add( ts_queue_del );
}

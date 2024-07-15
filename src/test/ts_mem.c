#include "common.h"
#include "test_main.h"

static ezhash_t * g_ts_hash;

static void ts_ezhash_init( void )
{
    int ret = ezhash_create( &g_ts_hash, 64 );
    t_assert( OK == ret );
}

static void ts_ezhash_deinit( void ) 
{
    int ret = ezhash_free( g_ts_hash );
    t_assert( OK == ret );
}

static void ts_ezhash_add( void )
{
    int ret = 0;
    ret = ezhash_add( g_ts_hash, "1", 1, "0", 1);
    t_assert(OK == ret);
    ret = ezhash_add( g_ts_hash, "2", 1, "0", 1);
    t_assert(OK == ret);
    ret = ezhash_add( g_ts_hash, "3", 1, "0", 1);
    t_assert(OK == ret);
    ret = ezhash_add( g_ts_hash, "4", 1, "0", 1);
    t_assert(OK == ret);
    ret = ezhash_add( g_ts_hash, "5", 1, "0", 1);
    t_assert(OK == ret);
}

static void ts_ezhash_del( void )
{
    int ret = 0;
    ret = ezhash_del( g_ts_hash, "1", 1);
    t_assert( OK == ret );
}

void ts_mem_init( )
{
    /// test ezhash 
    test_add( ts_ezhash_init );

    test_add(ts_ezhash_add);
    test_add(ts_ezhash_del);
    
    test_add( ts_ezhash_deinit );
}

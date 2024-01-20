#include "macro.h"
#include "test_main.h"

static manager_t manager;

static test_init_pt init_t[] = {
	ts_mem_init,
	ts_list_init,
	ts_queue_init,
	ts_bst_init,
	ts_bheap_init,
	ts_dns_init,
	NULL
};


status test_add ( test_pt pt )
{
	manager.test[manager.num].pt = pt;
	manager.num ++;
	return OK;
}

status test_run( void )
{
	uint32 i = 0;

	t_echo ( "=======================\n" );
	t_echo ( "\ttest start \n" );
	t_echo ( "=======================\n" );
	for( i = 0; i < manager.num; i ++ ) {
		manager.test[i].pt( );
	}
	t_echo ( "=======================\n" );
	t_echo ( "test uint num [%d]\n", manager.num );
	t_echo ( "=======================\n" );
	return OK;
}

status test_start( void )
{
	uint32 i;

	for( i = 0; init_t[i]; i ++ ) {
		init_t[i]( );
	}
	test_run();
	return OK;
}

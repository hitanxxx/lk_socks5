#include "lk.h"

static manager_t manager;

static test_init_pt init_t[] = {
	ts_mem_init,
	ts_list_init,
	ts_queue_init,
	ts_bst_init,
	ts_bheap_init,
	ts_str_init,
	ts_json_init,
	NULL
};
// test_add -----------------------------
status test_add ( test_pt pt )
{
	manager.test[manager.num].pt = pt;
	manager.num ++;
	return OK;
}
// test_run ------------------------------
status test_run( void )
{
	uint32 i = 0;

	debug_log ( "%s === run test ", __func__ );
	for( i = 0; i < manager.num; i ++ ) {
		manager.test[i].pt( );
	}
	debug_log ( "%s === run over", __func__ );
	debug_log ( "%s === all num [%d]", __func__, manager.num );
	debug_log ( "%s === failed num [%d]", __func__, failed_num );
	return OK;
}
// test_init ----------------------------
status test_init( void )
{
	uint32 i;

	for( i = 0; init_t[i]; i ++ ) {
		init_t[i]( );
	}
	return OK;
}
// test_end -----------------------------
status test_end (void)
{
	return OK;
}

#include "l_base.h"
#include "l_test.h"

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

	t_echo ( "run test \n" );
	for( i = 0; i < manager.num; i ++ ) {
		manager.test[i].pt( );
	}
	t_echo ( "run over\n" );
	t_echo ( "all num [%d]\n", manager.num );
	t_echo ( "failed num [%d]\n", failed_num );
	return OK;
}

int main(  )
{
	uint32 i;

	for( i = 0; init_t[i]; i ++ ) {
		init_t[i]( );
	}
	test_run();
	return OK;
}

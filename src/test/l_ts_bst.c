#include "l_base.h"
#include "l_test.h"
static bst_t  tree;
static bst_node_t arr[7], arr1[10];
// -------------------
static void ts_bst_start( void )
{
	bst_create( &tree, NULL );
}
// -------------------
static void ts_bst_add( void )
{
	t_assert( 0 == tree.elem_num );
	uint32 i = 0;

	memset( arr, 0, sizeof(arr) );
	arr[0].num = 20;
	arr[1].num = 18;
	arr[2].num = 24;
	arr[3].num = 10;
	arr[4].num = 19;
	arr[5].num = 22;
	arr[6].num = 25;

	while( i < 7 ) {
		bst_insert( &tree, &arr[i] );
		i ++;
	}
	t_assert( 7 == tree.elem_num );

	//bst_travesal_breadth( &tree );
}
// -------------------
static void ts_bst_del( void )
{
	uint32 i = 0;
	while( i < 7 ) {
		bst_del( &tree, &arr[i] );
		i++;
	}
	t_assert( NULL == tree.head.right );
	t_assert( 0 == tree.elem_num );
}
// -------------------
static void ts_bst_insert_repeat( void )
{
	status rc;

	t_assert( 0 == tree.elem_num );
	t_assert( NULL == tree.head.right );

	rc = bst_insert( &tree, &arr[0] );
	t_assert( rc == OK );
	t_assert( tree.elem_num == 1 );

	rc = bst_insert( &tree, &arr[0] );
	t_assert( rc == ERROR );
	t_assert( tree.elem_num == 1 );

	rc = bst_del( &tree, &arr[0] );
	t_assert( rc == OK );
	t_assert( tree.elem_num == 0 );
	t_assert( tree.head.right == NULL );
}
// -------------------
static void ts_bst_test_perform( void )
{
	uint32 i;
	bst_node_t * min;

	t_assert( tree.elem_num == 0 );
	t_assert( tree.head.right == NULL );

	memset( arr1, 0, sizeof(arr1) );
	for( i = 0; i < 10; i++ ) {
		bst_insert( &tree, &arr1[i] );
	}
	bst_travesal_breadth( &tree );
	for( i = 0; i < 10; i++ ) {
		bst_min( &tree, &min );
		bst_del( &tree, min );
	}
	t_assert( 0 == tree.elem_num );
	t_assert( NULL == tree.head.right );
}
// -------------------
static void ts_bst_reversal( void )
{
	arr[0].num = 1;
	arr[1].num = 2;
	arr[2].num = 3;
	arr[3].num = 4;
	arr[4].num = 5;

	bst_insert( &tree, &arr[2] );
	bst_insert( &tree, &arr[0] );
	bst_insert( &tree, &arr[3] );
	bst_insert( &tree, &arr[1] );
	bst_insert( &tree, &arr[4] );
	t_assert( tree.elem_num == 5 );
	t_assert( tree.head.right = &arr[2] );
	t_assert( arr[2].left = &arr[0] );
	t_assert( arr[2].right= &arr[3] );
	t_assert( arr[0].right = &arr[1] );
	t_assert( arr[3].right = &arr[4] );

	bst_reversal( &tree );
	t_assert( tree.elem_num == 5 );
	t_assert( tree.head.left = &arr[2] );
	t_assert( arr[2].left = &arr[3] );
	t_assert( arr[2].right = &arr[0] );

	t_assert( arr[3].left = &arr[4] );
	t_assert( arr[0].left = &arr[1] );
}
// -------------------
void ts_bst_init( )
{
	test_add( ts_bst_start );
	test_add( ts_bst_add );
	test_add( ts_bst_del );

	test_add( ts_bst_insert_repeat );

	test_add( ts_bst_test_perform );
	test_add( ts_bst_reversal );
}

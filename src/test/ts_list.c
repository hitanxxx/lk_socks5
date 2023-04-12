#include "common.h"
#include "test_main.h"

static mem_arr_t * list;
static string_t x = string("1");
static string_t y = string("2");
static string_t z = string("3");

// ---------------------------
static void ts_list_create( void )
{
	status rc;
	
	rc = mem_arr_create( &list, sizeof(string_t) );
	t_assert( rc == OK );
}
// -----------------------------
static void ts_list_push( void )
{
	string_t * a, *b, *c;
	
	a = mem_arr_push( list );
	t_assert( a );
	
	b = mem_arr_push( list );
	t_assert( b );
	
	c = mem_arr_push( list );
	t_assert( c );
	
	a->len = x.len;
	a->data = x.data;
	
	b->len = y.len;
	b->data = y.data;
	
	c->len = z.len;
	c->data = z.data;
}
// -----------------------------
static void ts_list_get( void )
{
	string_t * str;
	
	str = mem_arr_get( list, 1 );
	t_assert( str );
	t_assert( str->data == x.data );
	t_assert( str->len == x.len );
	
	str = (string_t*)mem_arr_get( list, 2 );
	t_assert( str );
	t_assert( str->data == y.data );
	t_assert( str->len == y.len );
	
	str = (string_t*)mem_arr_get( list, 3 );
	t_assert( str );
	t_assert( str->data == z.data );
	t_assert( str->len == z.len );
	
	str = (string_t*)mem_arr_get( list, 4 );
	t_assert( !str );
	
	str = (string_t*)mem_arr_get( list, 0 );
	t_assert( !str );
}
// ---------------------------
static void ts_list_check( void )
{
	t_assert( list->elem_num == 3 );
}
// ----------------------------
static void ts_list_free( void )
{
	status rc;
	
	rc = mem_arr_free( list );
	t_assert ( rc == OK ); 
}
// -----------------------
static void ts_list_get_null( void )
{
	char * c;
	status rc;
	
	rc = mem_arr_create( &list, sizeof(char) );
	t_assert( rc == OK );  
	
	c = mem_arr_get( list, 1 );
	t_assert( c == NULL );
	
	rc = mem_arr_free( list );
	t_assert( rc == OK );
}
// ---------------------------
void ts_list_init( )
{
	test_add( ts_list_create );
	test_add( ts_list_push );
	test_add( ts_list_get );
	test_add( ts_list_check );
	test_add( ts_list_free );
	test_add( ts_list_get_null );
}

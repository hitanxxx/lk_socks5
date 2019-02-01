#include "lk.h"

// ---------------------
static void json_test_empty ( void  )
{
	status rc;
	json_ctx_t * ctx;
	string_t string = string("");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == ERROR );

	rc = json_ctx_free( ctx );
	t_assert( rc == OK );
}
// ---------------------
static void json_test_num_integer ( void  )
{
	status rc;
	json_ctx_t *ctx;
	json_node_t *num;
	string_t string = string(" 1234 ");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == 1234 );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}


}
// ---------------------
static void json_test_num_decimal ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *num;
	string_t string = string(" 0.123 ");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == 0.123 );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_num_minus_integer ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *num;
	string_t string = string("  -1234 ");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == -1234 );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_num_minus_decimal ( void  )
{
	status rc;
	json_ctx_t *ctx;
	json_node_t *num;
	string_t string = string("  -0.123 ");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == -0.123 );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_true ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *true;
	string_t string = string("true");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &true );
		t_assert( true->type == JSON_TRUE );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// --------------------
static void json_test_false ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *false;
	string_t string = string("false");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &false );
		t_assert( false->type == JSON_FALSE );

		rc == json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_null ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *null;
	string_t string = string("null");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &null );
		t_assert( null->type == JSON_NULL );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_string ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *str;
	string_t string = string("\"hello world\"");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &str );
		t_assert( str->type == JSON_STR );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_array ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *arr, *e;
	string_t string = string("[ \"hello\" , true, 1, [] ]");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &arr );
		t_assert( arr->type == JSON_ARR );

		json_get_child( arr, 1, &e );
		t_assert( e->type == JSON_STR );
		t_assert( e->name.len == l_strlen("hello") );

		json_get_child( arr, 2, &e );
		t_assert( e->type == JSON_TRUE );

		json_get_child( arr, 3, &e );
		t_assert( e->type == JSON_NUM );
		t_assert( e->num == 1 );

		json_get_child( arr, 4, &e );
		t_assert( e->type == JSON_ARR );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_obj ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *obj, *e;
	string_t string = string("{ \"one\" : true, \"two\" : {}, \"three\" : [] }");
	//string_t string = string("{ \"one\" : true , \"two\" : false }");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &obj );
		t_assert( obj->type == JSON_OBJ );

		json_get_obj_child_by_name( obj, "one", l_strlen("one"), &e );
		t_assert( e->type == JSON_TRUE );

		json_get_obj_child_by_name( obj, "two", l_strlen("two"), &e );
		t_assert( e->type == JSON_OBJ );

		json_get_obj_child_by_name( obj, "three", l_strlen("three"), &e );
		t_assert( e->type == JSON_ARR );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_obj1 ( void  )
{
	status rc;
	json_ctx_t * ctx;
	json_node_t *obj, *e;
	string_t string = string("{ \"one\" : { \"1\" : 123 }, \"two\" : false }");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &obj );
		t_assert( obj->type == JSON_OBJ );

		json_get_obj_child_by_name( obj, "one", l_strlen("one"), &e );
		t_assert( e->type == JSON_OBJ );
		json_get_obj_child_by_name( e, "1", l_strlen("1"), &e );
		t_assert( e->type == JSON_NUM );
		t_assert( e->num == 123 );

		json_get_obj_child_by_name( obj, "two", l_strlen("two"), &e );
		t_assert( e->type == JSON_FALSE );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_decode ( void  )
{
	status rc;
	json_ctx_t * ctx;
	string_t string = string("{ \"1\" : [ { \"2\": false }, [ 1, 2, 3 ], true, false, null, 12345, -1 ], \"4\": 555 }");

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
}
// ---------------------
static void json_test_encode ( void  )
{
	status rc;
	json_ctx_t* ctx;
	json_node_t *e, *parent, *parent1;
	json_node_t * obj, *arr;
	uint32 i, j;

	string_t obj_arr[3] = { string("obj1"), string("obj2"), string("obj3") };
	string_t str_arr[3] = { string("string1"), string("string12"), string("string123") };
	meta_t * meta;

	/* {
		"obj1":[
			"string1",
			"string12",
			"string123"
		],
		"obj2":[
			"string1",
			"string12",
			"string123"
		],
		"obj3":[
			"string1",
			"string12",
			"string123"
		]
	} */
	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	e = json_add_obj( ctx, &ctx->root );
	parent = e;
	for( i = 0; i < 3; i++ ) {
		e = json_obj_add_child( ctx, parent, obj_arr[i].data, obj_arr[i].len );
		e = json_add_arr( ctx, e );
		parent1 = e;
		for( j = 0; j < 3; j ++ ) {
			json_add_str( ctx, parent1, str_arr[i].data, str_arr[i].len );
		}
	}
	rc = json_encode( ctx, &meta );
	t_assert( rc == OK );
	//t_echo( INFO, "%.*s", meta_len( meta->pos, meta->last ), meta->pos );

	if( rc == OK ) {
		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	rc = json_decode( ctx, meta->pos, meta->last );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( &ctx->root, 1, &obj );
		t_assert( obj->type == JSON_OBJ );

		json_get_obj_child_by_name( obj, "obj1", l_strlen("obj1"), &arr );
		t_assert( arr->type == JSON_ARR );

		json_get_child( arr, 1, &e );
		t_assert( e->type == JSON_STR );
		t_assert( e->name.len == l_strlen("string1") );

		rc = json_ctx_free( ctx );
		t_assert( rc == OK );
	}
	meta_free( meta );
}
// json_test_encode1 -------
static void json_test_encode1( void )
{
	/*
	{
		"1":"123",
		"2":"234",
		"3":"345"
	}
	*/
	json_ctx_t * ctx;
	json_node_t* obj, *v;
	meta_t * meta;
	status rc;
	string_t key[] = { string("1"), string("2"), string("3") };
	string_t val[] = { string("123"), string("234"), string("345") };

	rc = json_ctx_create( &ctx );
	t_assert( rc == OK );

	obj = json_add_obj( ctx, &ctx->root );
	v = json_obj_add_child( ctx, obj, key[0].data, key[0].len );
	json_add_str( ctx, v, key[0].data, key[0].len );

	v = json_obj_add_child( ctx, obj, key[1].data, key[1].len );
	json_add_str( ctx, v, key[1].data, key[1].len );

	v = json_obj_add_child( ctx, obj, key[2].data, key[2].len );
	json_add_str( ctx, v, key[2].data, key[2].len );

	rc = json_encode( ctx, &meta );
	t_assert( rc == OK );

	rc = json_ctx_free( ctx );
	t_assert( rc == OK );

	meta_free( meta );
}
// ---------------------
void ts_json_init( )
{
	test_add( json_test_empty );

	test_add( json_test_num_integer );
	test_add( json_test_num_decimal );
	test_add( json_test_num_minus_integer );
	test_add( json_test_num_minus_decimal );

	test_add( json_test_true );
	test_add( json_test_false );
	test_add( json_test_null );

	test_add( json_test_string );

	test_add( json_test_array );

	test_add( json_test_obj );
	test_add( json_test_obj1 );

	test_add( json_test_decode );

	test_add( json_test_encode );
	test_add( json_test_encode1 );
}

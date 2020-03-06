#include "l_base.h"
#include "l_test.h"
// -------------
static void ts_find_str( void )
{
	char * p;

	string_t string = string("12345");
	string_t find = string("34");

	p = l_find_str( string.data, string.len, find.data, find.len );
	t_assert( p );
	t_assert( *p == '3' );
}
// -------------
static void ts_atoi( void )
{
	status rc;
	int32 num;

	string_t str = string("12345");

	rc = l_atoi( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert( num == 12345 );
}
// -------------
static void ts_atoi_minus( void )
{
	status rc;
	int32 num;

	string_t str = string("-12345");

	rc = l_atoi( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert( num == -12345 );
}
// -------------
static void ts_atoi_error( void )
{
	status rc;
	int32 num;

	string_t str = string("0.45");

	rc = l_atoi( str.data, str.len, &num );
	t_assert( rc == ERROR );
}
// -------------
static void ts_atof( void )
{
	status rc;
	float num;

	string_t str = string("0.1234");

	rc = l_atof( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert(  num == (float)0.1234 );
}
// -------------
static void ts_atof_minus( void )
{
	status rc;
	float num;

	string_t str = string("-0.1234");

	rc = l_atof( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert(  num == (float)-0.1234 );
}
// -------------
static void ts_atof_error( void )
{
	status rc;
	float num;

	string_t str = string("-0.12x34");

	rc = l_atof( str.data, str.len, &num );
	t_assert( rc == ERROR );
}
// -------------
static void ts_hex2dec( void )
{
	status rc;
	int32 num;

	string_t str = string("0Xfff0");

	rc = l_hex2dec( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert( num == 65520 );
}
// -------------
static void ts_hex2dec_minus( void )
{
	status rc;
	int32 num;

	string_t str = string("-0Xfff0");

	rc = l_hex2dec( str.data, str.len, &num );
	t_assert( rc == OK );
	t_assert( num == -65520 );
}
// -------------
static void ts_hex2dec_error( void )
{
	status rc;
	int32 num;

	string_t str = string("-0Xfff#$0");

	rc = l_hex2dec( str.data, str.len, &num );
	t_assert( rc != OK );
}
// -----------
static void ts_strncmp_cap ( void )
{
	status rc;

	rc = l_strncmp_cap( "ABCD1", l_strlen("ABCD1") ,"abcd1", l_strlen("abcd1") );
	t_assert( rc == OK );
}
// --------------
void ts_str_init(  )
{
	test_add( ts_find_str );

	test_add( ts_atoi );
	test_add( ts_atoi_minus );
	test_add( ts_atoi_error );

	test_add( ts_atof );
	test_add( ts_atof_minus );
	test_add( ts_atof_error );

	test_add( ts_hex2dec );
	test_add( ts_hex2dec_minus );
	test_add( ts_hex2dec_error );

	test_add( ts_strncmp_cap );
}

#include "l_base.h"

static mem_list_t 	*api_list = NULL;

status serv_api_find( string_t * key, serv_api_handler * handler )
{
	uint32 i = 0;
	serv_api_t ** t, *s;

	if(! api_list )
	{
		return ERROR;
	}
	for( i = 0; i < api_list->elem_num; i ++ ) 
	{
		t = mem_list_get( api_list, i + 1 );
		s = *t;
		if( OK == l_strncmp_cap( s->name.data, s->name.len, key->data, key->len ) ) 
		{
			if( handler ) 
			{
				*handler = s->handler;
			}
			return OK;
		}
	}
	return ERROR;
}

status serv_api_register( serv_api_t * api )
{
	serv_api_t ** t = NULL;

	t = mem_list_push( api_list );
	*t = api;
	return OK;
}

status serv_init( void )
{
	if( OK != mem_list_create( &api_list, sizeof(serv_api_t*) ) )
	{
		err(" mem_list_create api_list\n" );
		return ERROR;
	}
	return OK;
}

status serv_end( void )
{
	if( api_list ) 
	{
		mem_list_free( api_list );
	}
	api_list = NULL;
	return OK;
}

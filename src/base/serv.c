#include "common.h"


static mem_arr_t 	* g_api_list = NULL;
static mem_page_t * g_api_page = NULL;

status serv_api_find( string_t * key, serv_api_handler * handler )
{
	uint32 i = 0;
	serv_api_t *s;

	if( !g_api_list )
	{
		return ERROR;
	}
	for( i = 0; i < g_api_list->elem_num; i ++ ) 
	{
		s = mem_arr_get( g_api_list, i + 1 );
		if( s == NULL )
		{
			continue;
        }
        
		if( (s->name.len == key->len) && (OK == strncasecmp( (char*)s->name.data, (char*)key->data, key->len ) ) ) 
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

status serv_api_register( const char * api_key, serv_api_handler handler )
{
	serv_api_t * new = NULL;

	new = mem_arr_push( g_api_list );
	if( !new )
	{
		err("g_api_list mem list push failed\n");
		return ERROR;
	}
	new->name.len     = l_strlen(api_key);
	new->name.data    = mem_page_alloc( g_api_page, new->name.len+1 );
	if( new->name.data == NULL )
	{
		err("page mem alloc failed\n");
		return ERROR;
	}
	memcpy( new->name.data, api_key, new->name.len );
	new->handler = handler;
	return OK;
}

status serv_init( void )
{
	if( OK != mem_arr_create( &g_api_list, sizeof(serv_api_t) ) )
	{
		err("g_api_list create failed\n" );
		return ERROR;
	}

	if( OK != mem_page_create( &g_api_page, L_PAGE_DEFAULT_SIZE ) )
	{
		err("g_api_page create failed\n");
		mem_arr_free( g_api_list );
		return ERROR;
	}
	return OK;
}

status serv_end( void )
{
	if( g_api_list ) 
	{
		mem_arr_free( g_api_list );
		g_api_list = NULL;
	}
	
	if( g_api_page )
	{
		mem_page_free( g_api_page );
		g_api_page = NULL;
	}
	return OK;
}

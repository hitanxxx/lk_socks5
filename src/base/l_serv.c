#include "l_base.h"

static mem_list_t 	*api_list = NULL;
static l_mem_page_t * g_page = NULL;

status serv_api_find( string_t * key, serv_api_handler * handler )
{
	uint32 i = 0;
	serv_api_t *s;

	if(! api_list )
	{
		return ERROR;
	}
	for( i = 0; i < api_list->elem_num; i ++ ) 
	{
		s = mem_list_get( api_list, i + 1 );
		if( s == NULL )
		{
			continue;
		}
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

status serv_api_register( const char * api_key, serv_api_handler handler )
{
	serv_api_t * p = NULL;

	p = mem_list_push( api_list );
	if( p == NULL )
	{
		err("mem list push failed\n");
		return ERROR;
	}
	p->name.len     = l_strlen(api_key);
	p->name.data    = l_mem_alloc( g_page, p->name.len+1 );
	if( p->name.data == NULL )
	{
		err("page mem alloc failed\n");
		return ERROR;
	}
	memcpy( p->name.data, api_key, p->name.len );
	p->handler = handler;
	return OK;
}

status serv_init( void )
	{
	if( OK != mem_list_create( &api_list, sizeof(serv_api_t) ) )
	{
		err(" mem_list_create api_list\n" );
		return ERROR;
	}

	if( OK != l_mem_page_create( &g_page, L_PAGE_DEFAULT_SIZE ) )
	{
		err("mem page create failed\n");
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
	if( g_page )
	{
		l_mem_page_free( g_page );
	}
	return OK;
}

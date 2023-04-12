#include "common.h"

status list_page_alloc( mem_page_t * page, uint32 size, mem_list_t ** out )
{
	mem_list_t * new = NULL;

	if( !page || size < 0 )
	{
		err("page null or size < 0\n");
		return ERROR;
	}

	new = mem_page_alloc( page, sizeof(mem_list_t)+size );
	if( !new )
	{
		err("mem page alloc memlist failed\n");
		return ERROR;
	}
	memset( new, 0, sizeof(mem_list_t)+size );

	if( out )
	{
		*out = new;
	}
	return OK;
}


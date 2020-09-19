#include "l_base.h"

status meta_file_alloc( meta_t ** meta, uint32 length )
{
	char * local_meta = NULL;
	meta_t * t = NULL;

	if( length <= 0 )
	{
		err("length <= 0");
		return ERROR;
	}
	if( meta == NULL )
	{
		err("meta NULL\n");
		return ERROR;
	}

	local_meta = (char*)l_safe_malloc( sizeof(meta_t) );
	if( !local_meta ) 
	{
		return ERROR;
	}
	memset( local_meta, 0, sizeof(meta_t) );

	t = (meta_t*)local_meta;
	t->next 		= NULL;

	t->file_flag 	= 1;
	t->file_pos 	= 0;
	t->file_last 	= length;

	*meta = (meta_t*)local_meta;
	return OK;
}

status meta_page_alloc ( l_mem_page_t * page, uint32 size, meta_t ** out )
{
	char * local_meta = NULL;
	meta_t * t = NULL;

	if( page == NULL )
	{
		err("page NULL\n");
		return ERROR;
	}
	if( size <= 0 )
	{	
		err("size <= 0\n");
		return ERROR;
	}

	local_meta = (char*)l_mem_alloc( page, sizeof(meta_t)+size );
	if( !local_meta ) 
	{
		return ERROR;
	}
	memset( local_meta, 0, sizeof(meta_t)+size );

	t = (meta_t*)local_meta;
	t->start 	= t->pos = t->last = t->data;
	t->end 		= t->start + size;

	*out = (meta_t*)local_meta;
	return OK;
}

status meta_page_get_all( l_mem_page_t * page, meta_t * in, meta_t ** out )
{
	uint32 len = 0, part_len = 0;
	meta_t *all = NULL, * cl = in;
	char * p = NULL;

	if( page == NULL )
	{
		err("page NULL\n");
		return ERROR;
	}
	if( in == NULL || out == NULL )
	{
		err("in or out NULL\n");
		return ERROR;
	}

	while( cl ) 
	{
		len += meta_len( cl->pos, cl->last );
		cl = cl->next;
	}
	if( OK != meta_page_alloc( page, len, &all ) ) 
	{
		err(" alloc meta mem\n" );
		return ERROR;
	}
	p = all->data;
	cl = in;
	while( cl ) 
	{
		part_len = meta_len( cl->pos, cl->last );
		memcpy( p, cl->pos, part_len );
		p += part_len;
		cl = cl->next;
	}
	all->last += len;
	*out = all;
	return OK;
}

status meta_alloc( meta_t ** meta, uint32 size )
{
	char * local_meta = NULL;
	meta_t * t = NULL;

	if( size < 0 )
	{
		err("size <= 0\n" );
		return ERROR;
	}
	if( meta == NULL )
	{
		err("meta NULL\n");
		return ERROR;
	}
	
	local_meta = (char*)l_safe_malloc( sizeof(meta_t)+size );
	if( !local_meta ) 
	{
		return ERROR;
	}
	memset( local_meta, 0, sizeof(meta_t)+size );

	t = (meta_t*)local_meta;
	t->start 	= t->pos = t->last = t->data;
	t->end 		= t->start + size;

	*meta = (meta_t*)local_meta;
	return OK;
}

status meta_free( meta_t * meta )
{
	if( meta )
	{
		l_safe_free(meta);
	}
	return OK;
}

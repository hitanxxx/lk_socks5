#include "l_base.h"

status meta_page_alloc ( l_mem_page_t * page, uint32 size, meta_t ** out )
{
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

	t = l_mem_alloc( page, sizeof(meta_t)+size );
	if( NULL == t ) 
	{
		err("meta alloc failed\n");
		return ERROR;
	}
	memset( t, 0, sizeof(meta_t)+size );

	t->start 	= t->pos = t->last = t->data;
	t->end 		= t->start + size;

	*out = t;
	return OK;
}

status meta_page_get_all( l_mem_page_t * page, meta_t * in, meta_t ** out )
{
	uint32 len = 0, part_len = 0;
	meta_t *cl = in, *all = NULL;
	unsigned char * p = NULL;

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
		err("meta alloc all failed\n" );
		return ERROR;
	}
	p = all->pos;
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
	meta_t * t = NULL;

	if( meta == NULL )
	{
		err("meta NULL\n");
		return ERROR;
	}
	
	t = l_safe_malloc( sizeof(meta_t)+size );
	if( NULL == t ) 
	{
		return ERROR;
	}
	memset( t, 0, sizeof(meta_t)+size );
	
	t->start 	= t->pos = t->last = t->data;
	t->end 		= t->start + size;

	*meta = t;
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

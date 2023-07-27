#include "common.h"


status meta_alloc_form_mempage ( mem_page_t * page, uint32 size, meta_t ** out )
{
	meta_t * new = NULL;

	if( page == NULL )
	    return ERROR;
	if( size <= 0 )
		return ERROR;

	new = mem_page_alloc( page, sizeof(meta_t)+size );
	if( !new ) {
		err("meta alloc from page failed\n");
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t)+size );

	new->start 		= new->pos = new->last = new->data;
	new->end 		= new->start + size;

	*out = new;
	return OK;
}


status meta_alloc( meta_t ** meta, uint32 size )
{
	meta_t * t = NULL;

	if( meta == NULL )
		return ERROR;
	
	t = l_safe_malloc( sizeof(meta_t)+size );
	if( NULL == t ) {
		return ERROR;
	}
	memset( t, 0, sizeof(meta_t)+size );
	
	t->start 	= t->pos = t->last = t->data;
	t->end 		= t->start + size;

	*meta = t;
	return OK;
}

void meta_free( meta_t * meta )
{
	if( meta )
		l_safe_free(meta);
}

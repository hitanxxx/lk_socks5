	#include "common.h"


	status meta_alloc_form_mempage ( mem_page_t * page, uint32 size, meta_t ** out )
	{
	meta_t * meta_alloc = NULL;

	sys_assert(page != NULL);
	sys_assert(size > 0);

	meta_alloc = mem_page_alloc( page, sizeof(meta_t)+size );
	if( !meta_alloc ) {
		err("meta alloc from page failed\n");
		return ERROR;
	}
	memset( meta_alloc, 0, sizeof(meta_t)+size );

	meta_alloc->start = meta_alloc->pos = meta_alloc->last = meta_alloc->data;
	meta_alloc->end = meta_alloc->start + size;

	*out = meta_alloc;
	return OK;
	}


	status meta_alloc( meta_t ** meta, uint32 size )
	{
	meta_t * t = NULL;

	if( meta == NULL ) {
		return ERROR;
	}

	t = l_safe_malloc( sizeof(meta_t)+size );
	if( NULL == t ) {
		return ERROR;
	}

	t->start = t->pos = t->last = t->data;
	t->end = t->start + size;

	*meta = t;
	return OK;
	}

	void meta_free( meta_t * meta )
	{
	if( meta ) {
		l_safe_free(meta);
	}
	}

#include "l_base.h"

// meta_file_alloc ------------
status meta_file_alloc( meta_t ** meta, uint32 length )
{
	meta_t * new = NULL;

	new = (meta_t*)l_safe_malloc( sizeof(meta_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t) );
	new->data = NULL;
	new->next = NULL;

	new->file_flag = 1;
	new->file_pos = 0;
	new->file_last = length;

	*meta = new;
	return OK;
}
// meta_page_alloc ----------
status meta_page_alloc ( l_mem_page_t * page, uint32 size, meta_t ** out )
{
	meta_t * new = NULL;

	new = (meta_t*)l_mem_alloc( page, sizeof(meta_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t) );
	new->data = NULL;
	new->next = NULL;

	new->data = (char*)l_mem_alloc( page, size*sizeof(char) );
	if( !new->data ) {
		return ERROR;
	}
	memset( new->data, 0, size*sizeof(char) );
	new->start = new->pos = new->last = new->data;
	new->end = new->start + (size*sizeof(char));

	*out = new;
	return OK;
}
// meta_page_get_all ---------
status meta_page_get_all( l_mem_page_t * page, meta_t * in, meta_t ** out )
{
	uint32 len = 0, part_len = 0;
	meta_t * cl, *new;
	char * p = NULL;

	cl = in;
	while( cl ) {
		len += meta_len( cl->pos, cl->last );
		cl = cl->next;
	}
	if( OK != meta_page_alloc( page, len, &new ) ) {
		err(" alloc meta mem\n" );
		return ERROR;
	}
	p = new->data;
	cl = in;
	while( cl ) {
		part_len = meta_len( cl->pos, cl->last );
		memcpy( p, cl->pos, part_len );
		p += part_len;
		cl = cl->next;
	}
	new->last += len;
	*out = new;
	return OK;
}
// meta_alloc --------------
status meta_alloc( meta_t ** meta, uint32 size )
{
	meta_t * new = NULL;

	new = (meta_t*)l_safe_malloc( sizeof(meta_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t) );
	new->data = NULL;
	new->next = NULL;

	new->data = (char*)l_safe_malloc( size*sizeof(char) );
	if( !new->data ) {
		l_safe_free( new );
		return ERROR;
	}
	memset( new->data, 0, size*sizeof(char) );
	new->start = new->pos = new->last = new->data;
	new->end = new->start + size*sizeof(char);

	*meta = new;
	return OK;
}
// meta_free ---------------
status meta_free( meta_t * meta )
{
	if( !meta->file_flag ) {
		if( meta->data ) {
			l_safe_free( meta->data );
		}
	}
	l_safe_free( meta );
	meta = NULL;
	return OK;
}

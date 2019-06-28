#ifndef _L_META_H_INCLUDED_
#define _L_META_H_INCLUDED_

#define meta_len( start, end )	( (uint32)( (char*)end - (char*)start ) )
typedef struct l_meta_t meta_t;
typedef struct l_meta_t {

	// flag
	uint32				file_flag;

	// memory meta
	char * data;
	char * start;
	char * pos;
	char * last;
	char * end;
	// file meta
	uint32				file_pos;
	uint32				file_last;

	meta_t* 			next;

} l_meta_t;

status meta_file_alloc( meta_t ** meta, uint32 length );
status meta_page_alloc( l_mem_page_t * page, uint32 size, meta_t ** out );
status meta_page_get_all( l_mem_page_t * page, meta_t * in, meta_t ** out );
status meta_alloc( meta_t ** meta, uint32 size );
status meta_free( meta_t * meta );

#endif

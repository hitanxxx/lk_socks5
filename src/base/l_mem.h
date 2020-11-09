#ifndef _L_MEM_H_INCLUDED_
#define _L_MEM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define L_PAGE_DEFAULT_SIZE 4096

typedef struct l_mem_page l_mem_page_t;
struct l_mem_page {
	uint32			size;

	// start pointer use position, end pointer usable potion
    char 			*start, *end;
    l_mem_page_t 	*next;
	char 			data[0];
};

status l_mem_page_create( l_mem_page_t ** alloc, uint32 size );
status l_mem_page_free( l_mem_page_t * page );
void * l_mem_alloc( l_mem_page_t * page, uint32 size );

#ifdef __cplusplus
}
#endif

#endif

#ifndef _MEM_H_INCLUDED_
#define _MEM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define L_PAGE_DEFAULT_SIZE 4096

typedef struct l_mem_page mem_page_t;
struct l_mem_page {
	uint32			size;

	// start pointer use position, end pointer usable potion
    char 			*start, *end;
    mem_page_t 	*next;
	char 			data[0];
};

status mem_page_create( mem_page_t ** alloc, uint32 size );
status mem_page_free( mem_page_t * page );
void * mem_page_alloc( mem_page_t * page, uint32 size );


char * sys_alloc( int size );
void sys_free(    char * addr );



#ifdef __cplusplus
}
#endif

#endif

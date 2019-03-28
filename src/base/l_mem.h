#ifndef _L_MEM_H_INCLUDED_
#define _L_MEM_H_INCLUDED_

#define L_PAGE_SIZE 4096

typedef struct l_mem_page_t l_mem_page_t;
struct l_mem_page_t {
    char *      data;
    char *      start;
    char *      end;

    l_mem_page_t * next;
};

status l_mem_page_create( l_mem_page_t ** alloc, uint32 size );
status l_mem_page_free( l_mem_page_t * page );
void * l_mem_alloc( l_mem_page_t * page, uint32 size );

#endif

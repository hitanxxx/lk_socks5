#include "common.h"


/*
	mamanger memory chain, easy to free
*/

status mem_page_create( mem_page_t ** page, uint32 size )
{
    mem_page_t * n_page = NULL;

    size = ( size < L_PAGE_DEFAULT_SIZE ? L_PAGE_DEFAULT_SIZE : size );

	if( !page ) {
		err("mem_page create alloc struct NULL\n");
		return ERROR;
	}

    n_page = (mem_page_t*)l_safe_malloc( sizeof(mem_page_t) + size );
    if( NULL == n_page ) {
        err("mem_page alloc failed, [%d]\n", errno );
        return ERROR;
    }
    memset( n_page, 0, sizeof(mem_page_t) + size );

	n_page->size		= size;
    n_page->start 	    = n_page->data;
    n_page->end 		= n_page->start + n_page->size;
    n_page->next 		= NULL;
	
    *page = n_page;
    return OK;
}

status mem_page_free( mem_page_t * page )
{
    mem_page_t *cur = NULL, *next = NULL;

	if( !page )
		return OK;

	cur = page;
    while( cur ) {
        next = cur->next;
        l_safe_free( cur );
        cur = next;
    }
    return OK;
}

void * mem_page_alloc( mem_page_t * page, uint32 size )
{
    mem_page_t *cur = NULL, *next = NULL, *last = NULL;
    mem_page_t *n_page = NULL;

	if( !page )
		return NULL;

	// loop page chain find space
	cur = page;
	while( cur ) {
        last = cur;
		next = cur->next;
		if( cur->end - cur->start >= size ) {
			cur->start += size;
			return cur->start - size;
		}
		cur = next;
	}

	// alloc new page 
	if( OK != mem_page_create( &n_page, size ) ) {
		err("alloc new page failed\n");
		return NULL;
	}
    
	last->next = n_page;

	n_page->start += size;
	return n_page->start - size;
}

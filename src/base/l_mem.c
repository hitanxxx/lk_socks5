#include "l_base.h"

status l_mem_page_create( l_mem_page_t ** alloc, uint32 size )
{
    char * p = NULL;
    uint32 length = 0;
    l_mem_page_t * page;

    length = (size > L_PAGE_SIZE) ? size : L_PAGE_SIZE;
    p = l_safe_malloc(sizeof(l_mem_page_t) + length );
    if( !p ) 
	{
        err(" l_safe_malloc failed, [%d]\n", errno );
        return ERROR;
    }
    memset( p, 0, sizeof(l_mem_page_t) + length );

    page = (l_mem_page_t*)p;
    page->next = NULL;
	
    page->start = page->data;
    page->end = page->start + length;
	
    *alloc = page;
    return OK;
}

status l_mem_page_free( l_mem_page_t * page )
{
    l_mem_page_t * n;

    while( page ) 
	{
        n = page->next;
        l_safe_free( page );
        page = n;
    }
    return OK;
}

void * l_mem_alloc( l_mem_page_t * page, uint32 size )
{
    l_mem_page_t * last_prev = NULL, *last = NULL;
    l_mem_page_t * new = NULL;
    uint32 length = 0;
    char * p;

	if( page == NULL )
	{
		err("page null\n");
		return NULL;
	}

    if( size > L_PAGE_SIZE )
	{
        if( OK != l_mem_page_create( &new, size ) ) 
		{
            err(" create page failed\n" );
            return NULL;
        }

        last = page;
        while( last->next ) 
		{
            last = last->next;
        }
        last->next = new;
		
        new->start += size;
        return new->data;
    } 
	else 
	{
        last = page;
        while( last ) 
		{
			if( (uint32) ( last->end - last->start ) >= size )
			{
				p = last->start;
				last->start += size;
				return p;
			}
			last_prev = last;
			last = last->next;
        }
        if( OK != l_mem_page_create( &new, L_PAGE_SIZE ) ) 
		{
            err(" create page failed\n" );
            return NULL;
        }
        last_prev->next = new;
		
        new->start += size;
        return new->data;
    }
}

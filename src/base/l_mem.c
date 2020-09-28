#include "l_base.h"

status l_mem_page_create( l_mem_page_t ** alloc, uint32 size )
{
    l_mem_page_t * page = NULL;

	if( NULL == alloc )
	{
		err("mem_page create alloc struct NULL\n");
		return ERROR;
	}

    page = (l_mem_page_t*)l_safe_malloc(sizeof(l_mem_page_t) + size );
    if( NULL == page ) 
	{
        err("mem_page malloc failed, [%d]\n", errno );
        return ERROR;
    }
    memset( page, 0, sizeof(l_mem_page_t) + size );

    page->next 		= NULL;
    page->start 	= page->data;
    page->end 		= page->start + size;
	page->size		= size;
	
    *alloc = page;
    return OK;
}

status l_mem_page_free( l_mem_page_t * page )
{
    l_mem_page_t *cur = page, *next = NULL;

	if( NULL == page )
	{
		err("mem_page free page NULL\n");
		return ERROR;
	}

    while( cur ) 
	{
        next = cur->next;
        l_safe_free( cur );
        cur = next;
    }
    return OK;
}

void * l_mem_alloc( l_mem_page_t * page, uint32 size )
{
    l_mem_page_t *last = NULL, *prev = NULL;
    l_mem_page_t *new = NULL;
    uint32 length = 0;
    char * p;

	if( NULL == page )
	{
		err("mem_alloc page NULL\n");
		return NULL;
	}

    if( size > page->size )
	{
        if( OK != l_mem_page_create( &new, size ) ) 
		{
            err("mem_alloc big page crerte failed\n");
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
	
    last = page;
    while( last ) 
	{
		if( (uint32) ( last->end - last->start ) >= size )
		{
			p = last->start;
			last->start += size;
			return p;
		}
		prev = last;
		last = last->next;
    }
    if( OK != l_mem_page_create( &new, page->size ) ) 
	{
        err(" create page failed\n" );
        return NULL;
    }
    prev->next 	= new;
	p = new->start;
    new->start += size;
    return p;
}

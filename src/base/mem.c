#include "common.h"

static pthread_mutex_t mem_th_lock = PTHREAD_MUTEX_INITIALIZER;

char * sys_alloc( int size )
{
    assert( size > 0 );
    pthread_mutex_lock( &mem_th_lock );
    char * addr = calloc( 1, size );
    pthread_mutex_unlock( &mem_th_lock );
    if( !addr ) {
        err("sys alloc failed. [%d]\n", errno );
        return NULL;
    }
    return addr;
}

void sys_free(   char * addr )
{
    if( addr ) {
        pthread_mutex_lock( &mem_th_lock );
        sys_free(addr);
        pthread_mutex_unlock( &mem_th_lock );
    }
}

/// @brief alloc a memory page chain
/// @param page 
/// @param size 
/// @return 
status mem_page_create( mem_page_t ** page, uint32 size )
{
    mem_page_t * page_new = NULL;

    size = ( size < L_PAGE_DEFAULT_SIZE ? L_PAGE_DEFAULT_SIZE : size );
    page_new = (mem_page_t*)l_safe_malloc( sizeof(mem_page_t) + size );
    if( !page_new ) {
        err("mem_page create failed, [%d]. [%s]\n", errno, strerror(errno) );
        return ERROR;
    }

    page_new->size = size;
    page_new->start = page_new->data;
    page_new->end = page_new->start + page_new->size;

    page_new->next = NULL;
    *page = page_new;
    return OK;
}

/// @brief free a memory page chain
/// @param page 
/// @return 
status mem_page_free( mem_page_t * page )
{
    mem_page_t *cur = page;
    mem_page_t *next= NULL;

    if( !page ) {
        return OK;
    }

    while( cur ) {
        next = cur->next;
        l_safe_free( cur );
        cur = next;
    }
    return OK;
}

/// @brief alloc memory object form memory page chain
/// @param page 
/// @param size 
/// @return 
void * mem_page_alloc( mem_page_t * page, uint32 size )
{
    mem_page_t *cur = page;
    mem_page_t *next = NULL;
    mem_page_t *last = NULL;
    
    mem_page_t *page_new = NULL;

    if( !page ) {
        return NULL;
    }
        
    /// traversal chain find remain space 
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
    if( OK != mem_page_create( &page_new, size ) ) {
        err("alloc new page failed\n");
        return NULL;
    }
    
    /// add new page into chain
    last->next = page_new;

    /// return memory addr
    page_new->start += size;
    return page_new->start - size;
}

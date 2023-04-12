#include "common.h"
#include "test_main.h"

static mem_page_t * page;

// ----------
static void ts_mem_alloc_page( void )
{
    status rc;

    rc = mem_page_create( &page, 4096 );
    t_assert( rc == OK );
}
// ----------
static void ts_mem_alloc_small( void )
{
    uint32 i = 0;

    for( i = 0; i < 4096; i ++ ) {
        mem_page_alloc( page, 1 );
    }
    t_assert( page->start == page->end );
}
// ----------
static void ts_mem_alloc_large( void )
{
    mem_page_t * n;

    mem_page_alloc( page, 8192 );

    n = page->next;
	t_assert( n != NULL );
    t_assert( n->next == NULL );
    t_assert( n->start == n->end );
	t_assert( ( n->end - n->data ) == 8192 );
}
// ---------
static void ts_mem_alloc_part( void )
{
    mem_page_t * n;

    mem_page_alloc( page, 3000 );

    n = page->next->next;
	t_assert( n != NULL );
    t_assert( n->start + 1096 == n->end );

    mem_page_alloc( page, 3000 );
    n = page->next->next->next;
	t_assert( n != NULL );
    t_assert( n->start + 1096 == n->end );
}
// ----------
static void ts_mem_free_page( void )
{
    status rc;

    rc = mem_page_free( page );
    page = NULL;
    t_assert( rc == OK );
}

// ----------
void ts_mem_init( )
{
    test_add( ts_mem_alloc_page );
    test_add( ts_mem_alloc_small );
    test_add( ts_mem_alloc_large );
    test_add( ts_mem_alloc_part );

    test_add( ts_mem_free_page );
}
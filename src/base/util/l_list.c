#include "l_base.h"

// list_create ----------------
status mem_list_create( mem_list_t ** list, uint32 size )
{
	mem_list_t * new = NULL;

	new = l_safe_malloc( sizeof(mem_list_t) );
	if( !new ) {
		return ERROR;
	}
	new->head = NULL;
	new->last = NULL;
	new->elem_size = size;
	new->elem_num = 0;

	*list = new;
	return OK;
}
// mem_list_push ------------
void * mem_list_push( mem_list_t * list )
{
	mem_list_part_t * new = NULL;

	new = l_safe_malloc( sizeof(mem_list_part_t) );
	if( !new ) {
		return NULL;
	}
	memset( new, 0, sizeof(mem_list_part_t) );
	new->next = NULL;
	new->data = NULL;

	new->data = l_safe_malloc( list->elem_size );
	if( !new->data ) {
		l_safe_free( new );
		return NULL;
	}
	memset( new->data, 0, list->elem_size );

	if( !list->head ) {
		list->head = new;
	}
	if( !list->last ) {
		list->last = new;
	} else {
		list->last->next = new;
		list->last = new;
	}

	list->elem_num++;
	return new->data;
}
// mem_list_free ---------------------------------------------
status mem_list_free( mem_list_t * list )
{
	mem_list_part_t * current, *next;

	current = list->head;
	while( current ) {
		next = current->next;
		l_safe_free( current->data );
		l_safe_free( current );
		current = next;
	}
	l_safe_free( list );
	return OK;
}
// mem_list_get ----------------------------------------------
void * mem_list_get( mem_list_t * list, uint32 index )
{
	uint32 i = 1;
	mem_list_part_t * find = NULL;

	if( index < 1 ) {
		return NULL;
	}
	if( index > list->elem_num ) {
		return NULL;
	}
	find = list->head;
	while( i < index ) {
		find = find->next;
		i++;
	}
	return find->data;
}

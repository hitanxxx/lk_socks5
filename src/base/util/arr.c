#include "common.h"

status mem_arr_create( mem_arr_t ** list, uint32 size )
{
	mem_arr_t *  narr = mem_pool_alloc( sizeof(mem_arr_t) );
	if( !narr ) {
		return ERROR;
	}
	narr->head = NULL;
	narr->last = NULL;
	narr->elem_size = size;
	narr->elem_num = 0;
	*list = narr;
	return OK;
}

void * mem_arr_push( mem_arr_t * list )
{
	mem_arr_part_t * narr = mem_pool_alloc( sizeof(mem_arr_part_t) + list->elem_size );
	if( NULL == narr ) {
		err("list alloc failed\n");
		return NULL;
	}
    
	if( 0 == list->elem_num ) {
        list->last = narr;
		list->head = narr;	
	} else {
		list->last->next = narr;
		list->last = narr;
	}
	list->elem_num += 1;
	return narr->data;
}

status mem_arr_free( mem_arr_t * list )
{
	mem_arr_part_t * cur, *next;

	cur = list->head;
	while( cur )  {
		next = cur->next;
		mem_pool_free( cur );
		cur = next;
	}
	mem_pool_free( list );
	return OK;
}

void * mem_arr_get( mem_arr_t * list, uint32 index )
{
	uint32 i = 1;
	mem_arr_part_t * head = NULL;

	if( (index < 1) || (index > list->elem_num) ) {
		return NULL;
	}
	head = list->head;
	while( i < index ) {
		head = head->next;
		i++;
	}
	return head->data;
}

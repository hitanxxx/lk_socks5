#include "l_base.h"

status mem_list_create( mem_list_t ** list, uint32 size )
{
	mem_list_t * new = NULL;

	new = l_safe_malloc( sizeof(mem_list_t) );
	if( !new ) 
	{
		return ERROR;
	}
	new->head 		= NULL;
	new->last 		= NULL;
	new->elem_size 	= size;
	new->elem_num 	= 0;

	*list = new;
	return OK;
}

void * mem_list_push( mem_list_t * list )
{
	mem_list_part_t * new = NULL;

	new = l_safe_malloc( sizeof(mem_list_part_t) + list->elem_size );
	if( NULL == new )
	{
		err("list alloc failed\n");
		return NULL;
	}
	memset( new, 0, sizeof(mem_list_part_t) + list->elem_size );
		
	if( 0 == list->elem_num )
	{
		list->head = list->last = new;	
	}
	else
	{
		list->last->next = new;
		list->last = new;
	}
	list->elem_num += 1;
	return new->data;
}

status mem_list_free( mem_list_t * list )
{
	mem_list_part_t * cur, *next;

	cur = list->head;
	while( cur ) 
	{
		next = cur->next;
		l_safe_free( cur );
		cur = next;
	}
	l_safe_free( list );
	return OK;
}

void * mem_list_get( mem_list_t * list, uint32 index )
{
	uint32 i = 1;
	mem_list_part_t * head = NULL;

	if( index < 1 || index > list->elem_num ) 
	{
		return NULL;
	}
	head = list->head;
	while( i < index ) 
	{
		head = head->next;
		i++;
	}
	return head->data;
}

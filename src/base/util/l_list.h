#ifndef _L_LIST_H_INCLUDED_
#define _L_LIST_H_INCLUDED_

typedef struct mem_list_part_t mem_list_part_t;
struct mem_list_part_t {
	void * data;
	mem_list_part_t * next;
};

typedef struct mem_list_t {
	uint32			elem_size;
	uint32			elem_num;
	mem_list_part_t		*head;
	mem_list_part_t 	*last;
} mem_list_t;

status mem_list_create( mem_list_t ** l, uint32	size );
void * mem_list_push( mem_list_t * l );
status mem_list_free( mem_list_t * l );
void * mem_list_get( mem_list_t * list, uint32 index );

#endif

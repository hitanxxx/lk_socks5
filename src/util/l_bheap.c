#include "lk.h"

// heap_create ----------
status heap_create( heap_t ** heap, uint32 size )
{
	heap_t * new;
	
	new = (heap_t*)l_safe_malloc( sizeof(heap_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(heap_t));
	new->array = NULL;
	
	new->array = (void*)l_safe_malloc( sizeof(heap_node_t*)*(size+1) );
	if( !new->array ) {
		l_safe_free( new ); 
		return ERROR;
	}
	memset( new->array, 0, sizeof(heap_node_t*)*(size+1) ); 
	
	new->size = size;
	new->index = 0;
	new->array[0] = NULL;
	*heap = new;
	return OK;
}

// heap_free ---------------
status heap_free( heap_t * heap )
{
	l_safe_free( heap->array );
	l_safe_free( heap );
	heap = NULL;
	return OK;
}

// heap_add -------------
status heap_add( heap_t * heap, heap_node_t * node )
{
	uint32 			i;
	heap_node_t * 	parent_node;;
	uint32 			parent_index;
	
	if( heap->index >= heap->size ) {
		return ERROR;
	}
	// do insert
	heap->index++;
	heap->array[heap->index] = node;
	heap->array[heap->index]->index = heap->index;
	
	// sort
	i = heap->index;
	while( HEAP_PARENT(i) && 
		heap->array[i]->num < heap->array[HEAP_PARENT(i)]->num )	
	{
		parent_index = heap->array[HEAP_PARENT(i)]->index;
		heap->array[HEAP_PARENT(i)]->index = i;
		heap->array[i]->index = parent_index;
		
		parent_node =  heap->array[HEAP_PARENT(i)];
		heap->array[HEAP_PARENT(i)] = heap->array[i];
		heap->array[i] = parent_node;
		
		i = HEAP_PARENT(i);
	}
	return OK;
}

// heap_del --------------
status heap_del( heap_t * heap, uint32 del_index )
{
	uint32  i;
	heap_node_t *tail_node, *parent_node;
	uint32 	min_index, parent_index;
	
	if( heap->index < 1 ) {
		return ERROR;
	}
	if( del_index > heap->index ) {
		return ERROR;
	}
	// do del
	tail_node = heap->array[heap->index];
	tail_node->index = del_index;
	heap->array[del_index] = tail_node;
	heap->index--;
	
	// sort
	i = del_index;
	while ( HEAP_LCHILD(i) <= heap->index ) {
		min_index = HEAP_LCHILD(i);
		if( HEAP_RCHILD(i) <= heap->index ) {
			if( heap->array[HEAP_RCHILD(i)]->num <= 
				heap->array[HEAP_LCHILD(i)]->num )
			{
				min_index = HEAP_RCHILD(i);
			}
		}
		
		if( heap->array[i]->num <= heap->array[min_index]->num ) {
			break;
		} else {
			parent_index = heap->array[i]->index;
			heap->array[i]->index = min_index;
			heap->array[min_index]->index = parent_index;
			
			parent_node = heap->array[i];
			heap->array[i] = heap->array[min_index];
			heap->array[min_index] = parent_node;
			
			i = min_index;
		}
	}
	return OK;
}

// heap_get_min -------------
heap_node_t * heap_get_min( heap_t * heap )
{
	if( heap->index < 1 ) {
		return NULL;
	}
	return heap->array[1];
}

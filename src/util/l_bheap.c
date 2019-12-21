#include "lk.h"

status heap_create( heap_t ** heap, uint32 size )
{
	heap_t * new_heap;
	size_t alloc_size = 0;
	
	alloc_size = sizeof(heap_t) + (sizeof(heap_node_t*) * (size+1) );
	new_heap = (heap_t*)l_safe_malloc( alloc_size );
	if( !new_heap ) 
	{
		return ERROR;
	}
	memset( new_heap, 0, alloc_size );
	
	new_heap->space = size;
	new_heap->index = 0;
	new_heap->array[0] = NULL;
	*heap = new_heap;
	return OK;
}

status heap_free( heap_t * heap )
{
	l_safe_free( heap );
	return OK;
}

status heap_add( heap_t * heap, heap_node_t * node )
{
	uint32 	i;
	
	if( heap->index >= heap->space ) 
	{
		return ERROR;
	}
	// do insert
	heap->index++;
	
	// heapsort
	i = heap->index;
	while( HEAP_PARENT(i) &&  node->key < heap->array[HEAP_PARENT(i)]->key )	
	{
		heap->array[i] 			= heap->array[HEAP_PARENT(i)];
		heap->array[i]->index 	= i;	 
		
		i = HEAP_PARENT(i);
	}
	heap->array[i] = node;
	heap->array[i]->index = i;
	
	return OK;
}

status heap_del( heap_t * heap, uint32 del_index )
{
	uint32  i;
	heap_node_t *tail_node, *parent_node;
	uint32 	child_min;
	
	if( heap->index < 1 || heap->space < 1) 
	{
		return ERROR;
	}
	if( del_index > heap->index ) 
	{
		return ERROR;
	}
	// do del
	tail_node = heap->array[heap->index];
	heap->index--;
	
	// sort
	i = del_index;
	while ( HEAP_LCHILD(i) <= heap->index ) 
	{
		child_min = HEAP_LCHILD(i);
		if( HEAP_RCHILD(i) <= heap->index ) 
		{
			child_min = ( heap->array[HEAP_RCHILD(i)]->key < child_min ) ? HEAP_RCHILD(i): child_min;
		}
		
		if( tail_node->key >= heap->array[child_min]->key )
		{
			heap->array[i] 		= heap->array[child_min];
			heap->array[i]->index = i;
			
			i = child_min;
		}
		else
		{
			break;
		}
	}
	heap->array[i] = tail_node;
	heap->array[i]->index = i;
	return OK;
}

status heap_empty( heap_t * heap )
{
	if( heap->space < 1 || heap->index < 1)
	{
		return OK;
	}
	return ERROR;
}

heap_node_t * heap_min( heap_t * heap )
{
	if( heap->index < 1 || heap->space < 1 ) 
	{
		return NULL;
	}
	return heap->array[1];
}

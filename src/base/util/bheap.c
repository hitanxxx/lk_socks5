#include "common.h"


#define HEAP_LCHILD(index) (index*2)
#define	HEAP_RCHILD(index) ((index*2)+1)
#define HEAP_PARENT(index) (index/2)

int heap_create( heap_t ** heap, int size)
{
	heap_t * new_heap;
	
	/// heap index 0 is alwsys unused 
	int alloc_size = sizeof(heap_t) + (sizeof(heap_node_t*) * (size+1));
	new_heap = (heap_t*)sys_alloc(alloc_size);
	if(!new_heap)
		return -1;
	
	new_heap->space = size;
	new_heap->index = 0;
	new_heap->array[0] = NULL;
	*heap = new_heap;
	return 0;
}

int heap_free(heap_t * heap)
{
	if(heap) {
		sys_free(heap);
	} 
	return 0;
}

int heap_add(heap_t * heap, heap_node_t * node)
{
	int	i = 0;
	/// out of space 
	if(heap->index >= heap->space)  {
		return -1;
	}

	heap->index++;
	i = heap->index;

	while(HEAP_PARENT(i) && node->key < heap->array[HEAP_PARENT(i)]->key) {
		heap->array[i] = heap->array[HEAP_PARENT(i)];
		heap->array[i]->index 	= i;	 
		
		i = HEAP_PARENT(i);
	}
	heap->array[i] = node;
	heap->array[i]->index = i;
	return 0;
}

int heap_del(heap_t * heap, int del_index)
{
	int i = 0;
	heap_node_t *tail_node;
	int child_min;

	if(heap_empty(heap)) {
		return -1;
	}
	if(del_index > heap->index) {
		return -1;
	}
	tail_node = heap->array[heap->index];
	heap->index--;
	
	i = del_index;
	while(HEAP_LCHILD(i) <= heap->index) {
		child_min = HEAP_LCHILD(i);
		if(child_min <= heap->index && HEAP_RCHILD(i) <= heap->index) {
			child_min = (heap->array[HEAP_RCHILD(i)]->key < heap->array[child_min]->key ) ? HEAP_RCHILD(i): child_min;
		}
		
		if(tail_node->key >= heap->array[child_min]->key) {
			heap->array[i] = heap->array[child_min];
			heap->array[i]->index = i;
			i = child_min;
		} else {
			break;
		}
	}
	heap->array[i] = tail_node;
	heap->array[i]->index = i;
	return 0;
}

int heap_empty(heap_t * heap)
{
	if(heap && (heap->space < 1 || heap->index < 1)) {
		return 1;
	}
	return 0;
}

heap_node_t * heap_min(heap_t * heap)
{
	if(heap->index < 1 || heap->space < 1)  {
		return NULL;
	}
	return heap->array[1];
}

int heap_num(heap_t * heap)
{
	return heap->index;
}

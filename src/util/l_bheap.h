#ifndef _L_BHEAP_H_INCLUDED_
#define _L_BHEAP_H_INCLUDED_

#define HEAP_LCHILD(index) ( index*2 )
#define	HEAP_RCHILD(index) ( ( index*2 ) + 1 )
#define HEAP_PARENT(index) ( index/2 )


typedef struct heap_node_t {
	int64_t 	num;
	uint32		index;
} heap_node_t;

// bheap
typedef struct beap_t {
	heap_node_t	** array;
	uint32	index;
	uint32	size;
}heap_t;

status 	heap_create( heap_t ** heap, uint32 size );
status 	heap_free( heap_t * heap );
status 	heap_add( heap_t * heap, heap_node_t * node );
status 	heap_del( heap_t * heap, uint32 index );
heap_node_t* heap_get_min( heap_t * heap );

#endif

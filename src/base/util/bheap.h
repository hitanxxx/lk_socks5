#ifndef _BHEAP_H_INCLUDED_
#define _BHEAP_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define HEAP_LCHILD(index) ( index*2 )
#define	HEAP_RCHILD(index) ( ( index*2 ) + 1 )
#define HEAP_PARENT(index) ( index/2 )

typedef struct heap_node_t 
{
	int64_t		key;
	uint32		index;
} heap_node_t;

// bheap
typedef struct heap_t 
{
	uint32	index;
	uint32	space;
	heap_node_t	*array[0];
} heap_t;

status 	heap_create( heap_t ** heap, uint32 size );
status 	heap_free( heap_t * heap );
status 	heap_add( heap_t * heap, heap_node_t * node );
status 	heap_del( heap_t * heap, uint32 position );
status  heap_empty( heap_t * heap );
int32 heap_num( heap_t * heap );
heap_node_t * heap_min( heap_t * heap );

#ifdef __cplusplus
}
#endif
        
#endif

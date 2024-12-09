#ifndef _BHEAP_H_INCLUDED_
#define _BHEAP_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    int64_t     key;
    int     index;
} heap_node_t;

// bheap
typedef struct {
    int    index;
    int    space;
    heap_node_t    *array[0];
} heap_t;

int    heap_create(heap_t ** heap, int size);
int    heap_free(heap_t * heap);
int    heap_add(heap_t * heap, heap_node_t * node);
int    heap_del(heap_t * heap, int position);
int heap_empty(heap_t * heap);
int heap_num(heap_t * heap);
heap_node_t * heap_min(heap_t * heap);

#ifdef __cplusplus
}
#endif
        
#endif

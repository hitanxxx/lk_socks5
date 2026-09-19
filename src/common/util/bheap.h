#ifndef _BHEAP_H_INCLUDED_
#define _BHEAP_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t index;
    uint64_t key;
} heap_node_t;

// bheap
typedef struct {
    uint32_t index;
    uint32_t space;
    heap_node_t *array[];
} heap_t;

int heap_create(heap_t **heap, uint32_t size);
int heap_free(heap_t *heap);
int heap_add(heap_t *heap, heap_node_t *node);
int heap_del(heap_t *heap, uint32_t position);
int heap_empty(heap_t *heap);
int heap_num(heap_t *heap);
heap_node_t *heap_min(heap_t *heap);

#ifdef __cplusplus
}
#endif

#endif

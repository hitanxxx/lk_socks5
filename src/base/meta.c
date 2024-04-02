#include "common.h"


status meta_alloc( meta_t ** meta, uint32 datan )
{
    meta_t * t = NULL;

    if( meta == NULL ) {
        return ERROR;
    }

    t = mem_pool_alloc( sizeof(meta_t)+datan );
    if(!t) {
        return ERROR;
    }

    t->start = t->pos = t->last = t->data;
    t->end = t->start + datan;

    *meta = t;
    return OK;
}

void meta_free( meta_t * meta )
{
    if( meta ) {
        mem_pool_free(meta);
    }
}

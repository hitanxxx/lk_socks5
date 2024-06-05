#include "common.h"

int meta_alloc(meta_t ** meta, int datan)
{
    if(meta) {
        meta_t * t = mem_pool_alloc(sizeof(meta_t)+datan);
        if(!t) {
            return -1;
        }
        t->start = t->pos = t->last = t->data;
        t->end = t->start + datan;
        *meta = t;
        return 0;
    }
    return -1;
}

void meta_free(meta_t * meta)
{
    if(meta)
        mem_pool_free(meta);
}

#include "common.h"

int meta_alloc(meta_t **meta, int datan) {
    if (meta) {
        meta_t *t = NULL;
        schk(t = mem_pool_alloc(sizeof(meta_t) + datan), return -1);
        t->start = t->pos = t->last = t->data;
        t->end = t->start + datan;
        *meta = t;
        return 0;
    }
    return -1;
}

void meta_free(meta_t *meta) {
    meta_t *m = meta;
    meta_t *n = NULL;

    while (m) {
        n = m->next;
        mem_pool_free(m);
        m = n;
    }
    return;
}

int meta_getlens(meta_t *m) {
    meta_t *n = m;
    int len = 0;

    while (n) {
        len += meta_getlen(n);
        n = n->next;
    }
    return len;
}

int meta_pdata(meta_t *meta, void *data, int datan) {
    schk(meta_getfree(meta) >= datan, return -1);
    memcpy(meta->last, data, datan);
    meta->last += datan;
    return 0;
}

meta_t * meta_dump(meta_t *meta) {
    if (!meta->next) return meta;

    meta_t *cur = NULL;
    meta_t *dump = NULL;
    int dumpn = meta_getlens(meta);
    schk(0 == meta_alloc(&dump, dumpn), return NULL);
    
    cur = meta;
    while (cur) {
        memcpy(dump->last, cur->pos, meta_getlen(cur));
        dump->last += meta_getlen(cur);
        cur = cur->next;
    }
    return dump;
}


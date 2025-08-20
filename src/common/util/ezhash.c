#include "common.h"

#define EZHS_FAV1A
///#define EZHS_MURMUR3

static uint32_t fnv1a_32(void *data, int datan) {
    uint8_t *p = data;
    uint32_t hash = 0x811c9dc5;
    uint32_t prime = 0x01000193;
    while (datan--) {
        hash ^= *p++;
        hash *= prime;
    }
    return hash;
}

int ezhash_create(ezhash_t **hash, int space) {
    int i = 0;
    ezhash_t *hs = mem_pool_alloc(sizeof(ezhash_t));
    if(!hs) {
        err("hs alloc err. [%d]\n", errno);
        return -1;
    }
    hs->arrn = space;
    hs->arr = mem_pool_alloc(hs->arrn * sizeof(queue_t));
    if (!hs->arr) {
        err("ezhash alloc arr err. [%d]\n", errno);
        mem_pool_free(hs);
        return -1;
    }
    for(i = 0; i < hs->arrn; i++) {
        queue_init(&hs->arr[i]);
    }
    *hash = hs;
    return 0;
}


int ezhash_free(ezhash_t *hash) {
    int i = 0;
    if (hash) {
        for (i = 0; i < hash->arrn; i++) {
            if (!queue_empty(&hash->arr[i])) {
                queue_t *p = queue_head(&hash->arr[i]);
                while (p != queue_tail(&hash->arr[i])) {
                    queue_t *n = queue_next(p);

                    ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
                    if (obj) {
                        if (obj->key) 
                            mem_pool_free(obj->key);
                        if (obj->val)
                            mem_pool_free(obj->val);
                        queue_remove(&obj->queue);
                        mem_pool_free(obj);
                    }
                    
                    p = n;
                }
            }
        }
        mem_pool_free(hash->arr);
        mem_pool_free(hash);
    }
    return 0;
}

int ezhash_del(ezhash_t *hash, void *key, int keyn) {
#if defined EZHS_FAV1A
    uint32_t hash_val = fnv1a_32(key, keyn);
    int idx = hash_val % hash->arrn;
#endif

    if(!queue_empty(&hash->arr[idx])) {
        queue_t *p= queue_head(&hash->arr[idx]);
        queue_t *n = NULL;
        while (p != queue_tail(&hash->arr[idx])) {
            n = queue_next(p);
            
            ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
            if (obj && obj->keyn == keyn 
                && !memcmp(obj->key, key, keyn)) {
                if (obj->key) 
                    mem_pool_free(obj->key);
                if (obj->val)
                    mem_pool_free(obj->val);
                queue_remove(&obj->queue);
                mem_pool_free(obj);
            }
            
            p = n;
        }
    }
    return 0;
}

void * ezhash_find(ezhash_t *hash, void *key, int keyn) {
#if defined EZHS_FAV1A
    uint32_t hash_value = fnv1a_32(key, keyn);
    int idx = hash_value % hash->arrn;
#endif

    if(!queue_empty(&hash->arr[idx])) {
        queue_t *p = queue_head(&hash->arr[idx]);
        queue_t *n = NULL;
        while (p != queue_tail(&hash->arr[idx])) {
            n = queue_next(p);
            
            ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
            if (obj && obj->keyn == keyn 
                && !memcmp(obj->key, key, keyn)) {
                return obj->val;
            }
            
            p = n;
        }
    }
    return NULL;
}

int ezhash_add(ezhash_t *hash, void *key, int keyn, void *val, int valn) {

    if(ezhash_find(hash, key, keyn)) {
        err("ezhash already exist. ignore add request\n");
        return 0;
    }

#if defined EZHS_FAV1A
    uint32_t hash_value = fnv1a_32(key, keyn);
    int idx = hash_value % hash->arrn;
#endif

    ///alloc memory insert into queue 
    ezhash_obj_t * hs = mem_pool_alloc(sizeof(ezhash_obj_t));
    if(!hs) {
        err("alloc hs err. [%d]\n", errno);
        return -1;
    }
    hs->keyn = keyn;
    hs->valn = valn;
    hs->key = mem_pool_alloc(keyn);
    if(!hs->key) {
        err("alloc hs key err. [%d]\n", errno);
        mem_pool_free(hs);
        return -1;
    }
    hs->val = mem_pool_alloc(valn);
    if(!hs->val) {
        err("alloc hs val err. [%d]\n", errno);
        mem_pool_free(hs->key);
        mem_pool_free(hs);
        return -1;
    }
    memcpy(hs->key, key, keyn);
    memcpy(hs->val, val, valn);

    queue_insert_tail(&hash->arr[idx], &hs->queue);
    return 0;
}




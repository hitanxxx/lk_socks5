#include "common.h"

static uint32_t fnv1a_32(void * data, int datan)
{
    uint8_t * p = data;
    uint32_t hash = 0x811c9dc5;
    uint32_t prime = 0x01000193;
    while(datan--) {
        hash ^= *p++;
        hash *= prime;
    }
    return hash;
}


int ezhash_create(ezhash_t ** hash, int space)
{
    ezhash_t * nhash = mem_pool_alloc(sizeof(ezhash_t));
    if(!nhash) {
        err("hash alloc failed. [%d]\n", errno);
        return -1;
    }
    nhash->arrn = space;
    nhash->arr = mem_pool_alloc(nhash->arrn *sizeof(ezhash_obj_t*));
    if(!nhash->arr) {
        err("hash arr alloc failed\n");
        mem_pool_free(nhash);
        return -1;
    }
    *hash = nhash;
    return 0;
}


int ezhash_free(ezhash_t * hash)
{
    int i = 0;
    if(hash) {
        for(i = 0; i < hash->arrn; i++) {
            ezhash_obj_t * p = hash->arr[i];
            while(p) {
                if(p->key) mem_pool_free(p->key);
                if(p->val) mem_pool_free(p->val);
                p = p->next;
            }
        }
        mem_pool_free(hash->arr);
        mem_pool_free(hash);
    }
    return 0;
}

int ezhash_del(ezhash_t * hash, void * key, int keyn)
{
    uint32_t hash_val = fnv1a_32(key, keyn);
    int idx = hash_val % hash->arrn;
    if(!hash->arr[idx]) {
        return -1;
    }
    ezhash_obj_t * p = hash->arr[idx];
    if(p->next)
        hash->arr[idx] = p->next;
    if(p) {
        if(p->key) mem_pool_free(p->key);
        if(p->val) mem_pool_free(p->val);
        mem_pool_free(p);
    }
    return 0;
}

int ezhash_add(ezhash_t * hash, void * key, int keyn, void * val, int valn)
{
    uint32_t hash_value = fnv1a_32(key, keyn);
    int idx = hash_value % hash->arrn;
    
    ezhash_obj_t * nhash = mem_pool_alloc(sizeof(ezhash_obj_t));
    if(!nhash) {
        err("nhash alloc failed\n");
        return -1;
    }
    nhash->next = NULL;
    nhash->keyn = keyn;
    nhash->key = mem_pool_alloc(keyn);
    if(!nhash->key) {
        err("hash key alloc failed\n");
        mem_pool_free(nhash);
        return -1;
    }
    memcpy(nhash->key, key, keyn);

    nhash->valn = valn;
    nhash->val = mem_pool_alloc(valn);
    if(!nhash->val) {
        err("hash val alloc failed\n");
        mem_pool_free(nhash->key);
        mem_pool_free(nhash);
        return -1;
    }
    memcpy(nhash->val, val, valn);

    if(!hash->arr[idx]) {
        hash->arr[idx] = nhash;
    } else {
        ezhash_obj_t * p = hash->arr[idx];
        while(p->next) {
            p = p->next;
        }
        p->next = nhash;
    }
    return 0;
}

void * ezhash_find(ezhash_t * hash, void * key, int keyn)
{
    uint32_t hash_value = fnv1a_32(key, keyn);
    int idx = hash_value  % hash->arrn;

    if(hash->arr[idx]) {
        ezhash_obj_t * p = hash->arr[idx];
        while (p) {
            if(p->keyn == keyn && !memcmp(p->key, key, keyn)) {
                return p->val;
            }
            p = p->next;
        }
    }
    return NULL;
}



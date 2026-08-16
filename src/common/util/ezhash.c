#include "common.h"

static uint32_t fnv1a_32(void *data, uint32_t datan) {
    uint8_t *p = data;
    uint32_t hash = 0x811c9dc5;
    uint32_t prime = 0x01000193;
    while (datan--) {
        hash ^= *p++;
        hash *= prime;
    }
    return hash;
}

int ezhash_create(ezhash_t **out, uint32_t space) {
    ezhash_t *hash = mem_pool_alloc(sizeof(ezhash_t));
    if (hash) {
        hash->arrn = space;
        hash->arr = mem_pool_alloc(space * sizeof(queue_t));
        if (hash->arr) {
            for (int i = 0; i < space; i++) queue_init(&hash->arr[i]);
            hash->hash_func = fnv1a_32;
            *out = hash;
            return 0;
        } else {
            mem_pool_free(hash);
        }
    }
    return -1;
}

int ezhash_free(ezhash_t *hash) {
    if (hash) {
        for (int i = 0; i < hash->arrn; i++) {
            if (!queue_empty(&hash->arr[i])) {
                queue_t *p = queue_head(&hash->arr[i]);
                while (p != queue_tail(&hash->arr[i])) {
                    queue_t *n = queue_next(p);

                    ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
                    if (obj) {
                        queue_remove(&obj->queue);
                        if (obj->key) mem_pool_free(obj->key);
                        if (obj->val) mem_pool_free(obj->val);
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

int ezhash_del(ezhash_t *hash, void *key, uint32_t keyn) {
    uint32_t idx = (hash->hash_func(key, keyn) % hash->arrn);
    uint8_t del_cont = 0;
    if (!queue_empty(&hash->arr[idx])) {
        queue_t *p = queue_head(&hash->arr[idx]);
        queue_t *n = NULL;
        while (p != queue_tail(&hash->arr[idx])) {
            n = queue_next(p);

            ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
            if (obj->keyn == keyn && !memcmp(obj->key, key, keyn)) {
                del_cont ++;
                queue_remove(&obj->queue);
                if (obj->key) mem_pool_free(obj->key);
                if (obj->val) mem_pool_free(obj->val);
                mem_pool_free(obj);
            }

            p = n;
        }
    }
    return ((del_cont > 0) ? 0 : -1);
}

void *ezhash_find(ezhash_t *hash, void *key, uint32_t keyn) {
    uint32_t idx = (hash->hash_func(key, keyn) % hash->arrn);

    if (!queue_empty(&hash->arr[idx])) {
        queue_t *p = queue_head(&hash->arr[idx]);
        queue_t *n = NULL;
        while (p != queue_tail(&hash->arr[idx])) {
            n = queue_next(p);

            ezhash_obj_t *obj = ptr_get_struct(p, ezhash_obj_t, queue);
            if (obj->keyn == keyn && !memcmp(obj->key, key, keyn)) {
                return obj->val;
            }

            p = n;
        }
    }
    return NULL;
}

int ezhash_add(ezhash_t *hash, void *key, uint32_t keyn, void *val, uint32_t valn) {
    if (ezhash_find(hash, key, keyn)) {
        return -1;
    }

    uint32_t idx = (hash->hash_func(key, keyn) % hash->arrn);
    /// alloc memory insert into queue
    ezhash_obj_t *obj = mem_pool_alloc(sizeof(ezhash_obj_t));
    if (!obj) {
        err("alloc new hash obj err. [%d]\n", errno);
        return -1;
    }
    
    obj->keyn = keyn;
    obj->valn = valn;
    obj->key = mem_pool_alloc(keyn);
    if (!obj->key) {
        err("alloc new hash obj's key space err. [%d]\n", errno);
        mem_pool_free(obj);
        return -1;
    }
    obj->val = mem_pool_alloc(valn);
    if (!obj->val) {
        err("alloc new hash obj's val space err. [%d]\n", errno);
        mem_pool_free(obj->key);
        mem_pool_free(obj);
        return -1;
    }
    memcpy(obj->key, key, keyn);
    memcpy(obj->val, val, valn);

    queue_insert_tail(&hash->arr[idx], &obj->queue);
    return 0;
}

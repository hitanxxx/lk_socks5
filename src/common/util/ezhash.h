#ifndef _EZHASH_H_INCLUDED_
#define _EZHASH_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*ezhash_func_t) (void *key, uint32_t keyn);
typedef struct ezhash_obj ezhash_obj_t;
struct ezhash_obj {
    queue_t queue;
    
    void *key;
    uint32_t keyn;
    
    void *val;
    uint32_t valn;
};

typedef struct {
    ezhash_func_t hash_func;
    uint32_t arrn;      /// recommand set to big prime
    queue_t *arr;       /// queue header of hash obj
} ezhash_t;

int ezhash_del(ezhash_t *hash, void *key, uint32_t keyn);
int ezhash_create(ezhash_t **hash, uint32_t space);
int ezhash_free(ezhash_t *hash);
int ezhash_add(ezhash_t *hash, void *key, uint32_t keyn, void *val, uint32_t valn);

void *ezhash_find(ezhash_t *hash, void *key, uint32_t keyn);

#ifdef __cplusplus
}
#endif

#endif

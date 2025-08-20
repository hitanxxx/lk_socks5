#ifndef _EZHASH_H_INCLUDED_
#define _EZHASH_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ezhash_obj ezhash_obj_t;
struct ezhash_obj {
    queue_t queue;
    
    void * key;
    int keyn;
    void * val;
    int valn;
};

typedef struct {
    int arrn;       ///recommand set to big prime
    queue_t *arr;   ///queue header of hash obj
} ezhash_t;

int ezhash_free(ezhash_t *hash);
int ezhash_create(ezhash_t **hash, int space);

void *ezhash_find(ezhash_t *hash, void *key, int keyn);
int ezhash_add(ezhash_t *hash, void *key, int keyn, void *val, int valn);
int ezhash_del(ezhash_t *hash, void *key, int keyn);


#ifdef __cplusplus
}
#endif

#endif



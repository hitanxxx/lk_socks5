#ifndef _EZHASH_H_INCLUDED_
#define _EZHASH_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ezhash_obj ezhash_obj_t;

struct ezhash_obj
{
    string_t key;
    string_t value;
    ezhash_obj_t * next;
};

typedef struct 
{  
    int range;
    ezhash_obj_t ** buckets;
} ezhash_t;

int ezhash_create( ezhash_t ** hash, int size );
int ezhash_free( ezhash_t * hash );

int ezhash_add( ezhash_t * hash, char * key, char * value );
int ezhash_del( ezhash_t * hash, char * key );
char * ezhash_find( ezhash_t * hash, char * key );

#ifdef __cplusplus
}
#endif
        
#endif



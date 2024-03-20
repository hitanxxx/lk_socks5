#include "common.h"

static uint32_t FNV1a(const uint8_t* data, size_t size) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < size; i++) {
        h ^= data[i];
        h *= 16777619;
    }
    return h;
}

int ezhash_create( ezhash_t ** hash, int range )
{
    ezhash_t * new_hash = mem_pool_alloc( sizeof(ezhash_t) );
    if( !new_hash ) {
        err("alloc new hash failed. [%d]\n", errno );
        return -1;
    }
    new_hash->range = range;
    
    new_hash->buckets = mem_pool_alloc( new_hash->range *sizeof(ezhash_obj_t*) );
    if( !new_hash->buckets ) {
        err("alloc new hash's buckets failed\n");
        mem_pool_free(new_hash);
        return -1;
    }
    *hash = new_hash;
    return 0;
}


int ezhash_free( ezhash_t * hash )
{
    if( hash ) {
        /// free list
        ezhash_obj_t * p = hash->buckets[0];
        while( p ) {            
            if(p->key.data) {
                mem_pool_free(p->key.data);
            }
            if(p->value.data) {
                mem_pool_free(p->value.data);
            }
            p = p->next;
        }
        mem_pool_free(hash->buckets);
        mem_pool_free(hash);
    }
    return 0;
}

int ezhash_add( ezhash_t * hash, char * key, char * value )
{
    uint32_t hash_value = FNV1a( (unsigned char*)key, strlen(key) );
    int idx = hash_value%hash->range;
    /// hash number -> range number 
    
    ezhash_obj_t * nhash = mem_pool_alloc( sizeof(ezhash_obj_t) );
    if(!nhash) {
        err("nhash alloc failed\n");
        return -1;
    }
    nhash->next = NULL;
    nhash->key.len = strlen(key);
    nhash->key.data = mem_pool_alloc( nhash->key.len + 1 );
    if(!nhash->key.data) {
        err("nhash alloc key data failed\n");
        mem_pool_free(nhash);
        return -1;
    }
    strncpy( (char*)nhash->key.data, key, nhash->key.len );

    nhash->value.len = strlen(value);
    nhash->value.data = mem_pool_alloc( nhash->value.len + 1 );
    if(!nhash->value.data) {
        err("nhash alloc value data failed\n");
        mem_pool_free(nhash->key.data);
        mem_pool_free(nhash);
        return -1;
    }
    strncpy( (char*)nhash->value.data, value, nhash->value.len );

    if( hash->buckets[idx] == NULL ) {
        hash->buckets[idx] = nhash;
    } else {
        ezhash_obj_t * p = hash->buckets[idx];
        while(p->next) {
            p = p->next;
        }
        p->next = nhash;
    }
    return 0;
}


char * ezhash_find( ezhash_t * hash, char * key )
{
    uint32_t hash_value = FNV1a( (unsigned char*)key, strlen(key) );
    int idx = hash_value%hash->range;

    if( hash->buckets[idx] ) {
        ezhash_obj_t * p = hash->buckets[idx];
        while ( p ) {
            if( strlen(key) == p->key.len && strncmp( (char*)p->key.data, key, strlen(key) ) == 0 ) {
                return (char*)p->value.data;
            }
            p = p->next;
        }
    }
    return NULL;
}



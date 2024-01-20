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
    ezhash_t * new_hash = l_safe_malloc( sizeof(ezhash_t) );
    if( !new_hash ) {
        err("alloc new hash failed. [%d]\n", errno );
        return -1;
    }
    if( OK != mem_page_create( &new_hash->page, 4096 ) ) {
        err("alloc new hash's mem page failed\n");
        l_safe_free(new_hash);
        return -1;
    }
    new_hash->range = range;
    new_hash->buckets = mem_page_alloc( new_hash->page, new_hash->range *sizeof(ezhash_obj_t*) );
    if( !new_hash->buckets ) {
        err("alloc new hash's buckets failed\n");
        mem_page_free(new_hash->page);
        l_safe_free(new_hash);
        return -1;
    }
    *hash = new_hash;
    return 0;
}


int ezhash_free( ezhash_t * hash )
{
    if( hash ) {
        if( hash->page ) {
            mem_page_free(hash->page);
        }
        l_safe_free(hash);
    }
    return 0;
}

int ezhash_add( ezhash_t * hash, char * key, char * value )
{
    uint32_t hash_value = FNV1a( (unsigned char*)key, strlen(key) );
    int idx = hash_value%hash->range;
    /// hash number -> range number 

    if( hash->buckets[idx] == NULL ) {
        hash->buckets[idx] = mem_page_alloc( hash->page, sizeof(ezhash_obj_t) );
        if( !hash->buckets[idx] ) {
            err("ezhash mem page alloc obj failed\n");
            return -1;
        }
        hash->buckets[idx]->next = NULL;

        
        hash->buckets[idx]->key.data = mem_page_alloc( hash->page, strlen(key)+1 );
        if( !hash->buckets[idx]->key.data ) {
            err("ezhash mem page alloc obj key failed\n");
            return -1;
        }
        hash->buckets[idx]->key.len = strlen(key);
        strncpy( (char*)hash->buckets[idx]->key.data, key, strlen(key) );
    
        
        hash->buckets[idx]->value.data = mem_page_alloc( hash->page, strlen(value)+1 );
        if( !hash->buckets[idx]->value.data ) {
            err("ezhash mem page alloc obj value failed\n");
            return -1;
        }
        hash->buckets[idx]->value.len = strlen(value);
        strncpy( (char*)hash->buckets[idx]->value.data, value, strlen(value) );
        
    } else {
        ezhash_obj_t * new_obj = mem_page_alloc( hash->page, sizeof(ezhash_obj_t) );
        if ( !new_obj ) {
            err("ezhash mem page alloc obj failed\n");
            return -1;
        }
        new_obj->next = NULL;

        
        new_obj->key.data = mem_page_alloc( hash->page, strlen(key)+1 );
        if( !new_obj->key.data ) {
            err("ezhash mem page alloc obj key failed\n");
            return -1;
        }
        new_obj->key.len = strlen(key);
        strncpy( (char*)new_obj->key.data, key, strlen(key) );

        
        new_obj->value.data = mem_page_alloc( hash->page, strlen(value)+1 );
        if( !new_obj->value.data ) {
            err("ezhash mem page alloc obj value failed\n");
            return -1;
        }
        new_obj->value.len = strlen(value);
        strncpy( (char*)new_obj->value.data, value, strlen(value) );
        
        ezhash_obj_t * p = hash->buckets[idx];
        if( p->next ) {
            p = p->next;
        }
        p->next = new_obj;
    }
    return 0;
}


char * ezhash_find( ezhash_t * hash, char * key )
{
    uint32_t hash_value = FNV1a( (unsigned char*)key, strlen(key) );
    int idx = hash_value%hash->range;

    if( hash->buckets[idx] ) {
        ezhash_obj_t * p = hash->buckets[idx];
        if( p ) {
            if( strlen(key) == p->key.len && strncmp( (char*)p->key.data, key, strlen(key) ) == 0 ) {
                return (char*)p->value.data;
            }
            p = p->next;
        }
    
    }
    return NULL;
}



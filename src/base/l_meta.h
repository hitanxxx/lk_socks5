#ifndef _L_META_H_INCLUDED_
#define _L_META_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

typedef struct l_meta_t meta_t;
typedef struct l_meta_t 
{
    meta_t* next;
    // memory pointer
    unsigned char *     start;
    unsigned char *     pos;
    unsigned char *     last;
    unsigned char *     end;
    unsigned char       data[0];
} l_meta_t;

status meta_page_alloc( l_mem_page_t * page, uint32 size, meta_t ** out );
status meta_page_get_all( l_mem_page_t * page, meta_t * in, meta_t ** out );
status meta_alloc( meta_t ** meta, uint32 size );
status meta_free( meta_t * meta );

#ifdef __cplusplus
}
#endif
        
#endif

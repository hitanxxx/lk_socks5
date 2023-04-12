#ifndef _META_H_INCLUDED_
#define _META_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

typedef struct meta meta_t;
struct meta
{
    meta_t* next;
    // memory pointer
    unsigned char *     start;
    unsigned char *     pos;
    unsigned char *     last;
    unsigned char *     end;
    unsigned char       data[0];
};

status meta_alloc_form_mempage( mem_page_t * page, uint32 size, meta_t ** out );
status meta_alloc( meta_t ** meta, uint32 size );
void meta_free( meta_t * meta );

#ifdef __cplusplus
}
#endif
        
#endif

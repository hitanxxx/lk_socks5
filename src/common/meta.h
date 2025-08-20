#ifndef _META_H_INCLUDED_
#define _META_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct meta meta_t;
struct meta {
    meta_t* next;
    // memory pointer
    unsigned char *     start;
    unsigned char *     pos;
    unsigned char *     last;
    unsigned char *     end;
    unsigned char       data[0];
};

#define meta_getfree(x)     ((x)->end - (x)->last)
#define meta_clr(x)         ((x)->pos = (x)->last = (x)->start)
#define meta_cap(x)         ((x)->end - (x)->start)
#define meta_getlen(x)      ((x)->last - (x)->pos)


int meta_alloc(meta_t ** meta, int size);
void meta_free(meta_t * meta);
meta_t * meta_dump(meta_t * meta);
int meta_getlens(meta_t * m);
int meta_pdata(meta_t * meta, void * data, int datan);


#ifdef __cplusplus
}
#endif
        
#endif

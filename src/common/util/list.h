#ifndef _LIST_H_INCLUDED_
#define _LIST_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct mem_list mem_list_t;
struct mem_list 
{
    mem_list_t             *next;
    int                 datan;
    char                 data[0];
};

int mem_list_push(mem_list_t ** h, char * data);
int mem_list_free(mem_list_t * h);



#ifdef __cplusplus
}
#endif
        
#endif


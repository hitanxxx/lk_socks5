#include "common.h"


int mem_list_free(mem_list_t * h)
{
    mem_list_t * p = h;
    mem_list_t * n = NULL;

    while(p) {
        n = p->next;
        mem_pool_free(p);
        p = n;
    }
    return 0;
}

int mem_list_push(mem_list_t ** h, char * data)
{
    int datan = strlen(data);
    mem_list_t * nl = NULL;
    schk(nl = mem_pool_alloc(sizeof(mem_list_t)+datan+1), return -1);
    nl->next = NULL;
    nl->datan = datan;
    memcpy(nl->data, data, datan);

    if(!*h) {
        *h = nl;
    } else {
        mem_list_t * p = (*h);
        while(p->next) {
            p = p->next;
        }
        p->next = nl;
    }
    return 0;
}




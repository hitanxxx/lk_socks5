#ifndef _MEM_H_INCLUDED_
#define _MEM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

void * sys_alloc(int size);
void sys_free(    void * addr);


int mem_pool_free(void * addr);
void * mem_pool_alloc(int size);
int mem_pool_deinit();
int mem_pool_init();
char * mem_pool_ver();


#ifdef __cplusplus
}
#endif

#endif

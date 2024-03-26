#ifndef _MEM_H_INCLUDED_
#define _MEM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define L_PAGE_DEFAULT_SIZE 4096

typedef struct l_mem_page mem_page_t;
struct l_mem_page {
	uint32			size;

	// start pointer use position, end pointer usable potion
    char 			*start, *end;
    mem_page_t 	*next;
	char 			data[0];
};



void * sys_alloc( int size );
void sys_free(     void * addr );


int mem_pool_free(void * addr);
void * mem_pool_alloc(int size);
int mem_pool_deinit();
int mem_pool_init();
char * mem_pool_ver();



#ifdef __cplusplus
}
#endif

#endif

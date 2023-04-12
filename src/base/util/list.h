#ifndef _LIST_H_INCLUDED_
#define _LIST_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct mem_list mem_list_t;
struct mem_list 
{
	mem_list_t 			*next;
	int 				datalen;
	char	 			data[0];
};

status list_page_alloc( mem_page_t * page, uint32 size, mem_list_t ** out );

#ifdef __cplusplus
}
#endif
        
#endif


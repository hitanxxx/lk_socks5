#ifndef _ARR_H_INCLUDED_
#define _ARR_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct mem_arr_part mem_arr_part_t;
struct mem_arr_part 
{
	mem_arr_part_t 		*next;
	char	 			data[0];
};
typedef struct mem_arr_t 
{
	uint32				elem_size;
	uint32				elem_num;
	mem_arr_part_t		*head;
	mem_arr_part_t 		*last;
} mem_arr_t;

// fixed-length
int mem_arr_create(mem_arr_t ** l, int size);
void * mem_arr_push(mem_arr_t * l);
int mem_arr_free(mem_arr_t * l);
void * mem_arr_get(mem_arr_t * list, int index);

#ifdef __cplusplus
}
#endif
        
#endif

#ifndef _RINGBUFFER_H_INCLUDED_
#define _RINGBUFFER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	int 	size;
	int 	data_len;
	char * 	pos;
	char * 	last;
	char *  start;
	char *  end;
	char	data[0];
} ringbuffer_t;



int ringbuffer_alloc(  ringbuffer_t ** rb, int size );
void ringbuffer_free(  ringbuffer_t * rb);
int ringbuffer_push( ringbuffer_t * rb, char * data, int len);
int ringbuffer_pull( ringbuffer_t * rb, int len, char * out_data, int * out_len );
int ringbuffer_empty(ringbuffer_t * rb );


#ifdef __cplusplus
}
#endif

#endif

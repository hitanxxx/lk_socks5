#ifndef _RINGBUFFER_H_INCLUDED_
#define _RINGBUFFER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    pthread_mutex_t lock;
	int space;
	char * start;
	char * end;
	char * pos;
	char * last;

	int datan;
	char data[0];
} ringbuffer_t;



int ringbuffer_alloc(  ringbuffer_t ** rb, int size);
void ringbuffer_free(  ringbuffer_t * rb);
int ringbuffer_push(ringbuffer_t *rb,unsigned char *data, int datan);
int ringbuffer_pull(ringbuffer_t *rb, int datan, unsigned char *data, int *outn);



#ifdef __cplusplus
}
#endif

#endif

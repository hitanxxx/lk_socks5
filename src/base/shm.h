#ifndef _SHM_H_INCLUDED_
#define _SHM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct shm_t {
    uint32      size;
    char *      data;
} sys_shm_t;

int sys_shm_alloc(sys_shm_t * shm, int size);
int sys_shm_free(sys_shm_t * shm);

#ifdef __cplusplus
}
#endif

    
#endif

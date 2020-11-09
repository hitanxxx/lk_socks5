#ifndef _L_SHM_H_INCLUDED_
#define _L_SHM_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


typedef struct l_shm_t {
    uint32      size;
    char *      data;
} l_shm_t;

status l_shm_alloc( l_shm_t * shm, uint32 size );
status l_shm_free( l_shm_t * shm );

#ifdef __cplusplus
}
#endif

    
#endif

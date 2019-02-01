#include "lk.h"

// l_shm_alloc ----------------
status l_shm_alloc( l_shm_t * shm, uint32 size )
{
    shm->data = (char*) mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    if( shm->data == MAP_FAILED ) {
        err_log("%s --- shm alloc failed, [%d]", __func__, errno );
        return ERROR;
    }
    memset( shm->data, 0, shm->size );
    shm->size = size;
    return OK;
}
// l_shm_free ---------------
status l_shm_free( l_shm_t * shm )
{
    munmap((void *) shm->data, shm->size );
    return OK;
}

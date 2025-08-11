#include "common.h"


int sys_shm_alloc(sys_shm_t *shm, int size) {
    shm->data = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    if(shm->data == MAP_FAILED) {
        err("shm alloc failed, [%d]\n", errno);
        return -1;
    }
    memset(shm->data, 0, shm->size);
    shm->size = size;
    return 0;
}

int sys_shm_free(sys_shm_t *shm) {
    munmap((void *)shm->data, shm->size);
    return 0;
}

#include "common.h"

//#define MEM_BY_SYSCALL


#if defined(MEM_BY_SYSCALL)
void * sys_alloc(int size) { return calloc(1, size); }
void sys_free(void * addr) { free(addr); }
int mem_pool_deinit() { return 0; }
int mem_pool_init() { return 0; }
int mem_pool_free(void * p) { sys_free(p); return 0; }
void * mem_pool_alloc(int size) { return sys_alloc(size); }
char * mem_pool_ver() { return "MEM_BY_SYSCALL"; }
#else

#define MP_OBJ_MAGIC   0xa100beef
#define MP_BLK_MAX     6
#define MP_OBJ_MAX     1024

static char * g_mp_ver = "mp_v2";
static pthread_mutex_t g_sys_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_mp_thread_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    queue_t queue;
    int magic;          ///for magic chk
    int blk_idx;
    char addr[0];
} mem_obj_t;

typedef struct {
    queue_t usable;
    queue_t inuse;
    int pn;
    void * p;
} mem_block_t;

typedef struct {
    /* byte
    |------------------------------|
    | 0  | 1  | 2  | 3  | 4  | 5   |
    |----|----|----|----|----|-----|
    | 512|1024|2048|4096|8192|16384|
    |------------------------------|
    */
    mem_block_t * blks;
} mem_ctx_t;

static mem_ctx_t g_mem_ctx;


void * sys_alloc(int size)
{
    assert(size > 0);
    pthread_mutex_lock(&g_sys_thread_lock);
    char * addr = malloc(size);
    pthread_mutex_unlock(&g_sys_thread_lock);
    schk(addr, return NULL);
    memset(addr, 0x0, size);
    return addr;
}

void sys_free(void * addr)
{
    if(addr) {
        pthread_mutex_lock(&g_sys_thread_lock);
        free(addr);
        pthread_mutex_unlock(&g_sys_thread_lock);
    }
}

int mem_pool_deinit()
{
    if(g_mem_ctx.blks) {
        int i = 0;
        for(i = 0; i < MP_BLK_MAX; i++) {
            if(g_mem_ctx.blks[i].p) {
                free(g_mem_ctx.blks[i].p);
            }
        }
        free(g_mem_ctx.blks);
    }
    return 0;
}

int mem_pool_init()
{
    int i = 0;
    int j = 0;
    
    schk(NULL != (g_mem_ctx.blks = malloc(sizeof(mem_block_t) * MP_BLK_MAX)), return -1);
    memset(g_mem_ctx.blks, 0x0, sizeof(mem_block_t)*MP_BLK_MAX);
   
    for(i = 0; i < MP_BLK_MAX; i++) {
        int obj_space = 0;
        int obj_n = MP_OBJ_MAX;
        if(i == 0) {
            obj_space = 512;   
            obj_n *= 2; ///small obj * 2
        } else if (i == 1) {
            obj_space = 1024;
        } else if (i == 2) {
            obj_space = 2048;
        } else if (i == 3) {
            obj_space = 4096;
        } else if (i == 4) {
            obj_space = 8192;
        } else if (i == 5) {
            obj_space = 16384; 
        }
        g_mem_ctx.blks[i].pn = ((sizeof(mem_obj_t) + obj_space) * obj_n);
        schk(NULL != (g_mem_ctx.blks[i].p = malloc(g_mem_ctx.blks[i].pn)), return -1);
        memset(g_mem_ctx.blks[i].p, 0x0, g_mem_ctx.blks[i].pn);
        
        queue_init(&g_mem_ctx.blks[i].usable);
        queue_init(&g_mem_ctx.blks[i].inuse);
        char * p = g_mem_ctx.blks[i].p;
        for(j = 0; j < obj_n; j++) {
            mem_obj_t * obj = (mem_obj_t*)p;
            obj->magic = MP_OBJ_MAGIC;
            obj->blk_idx = i;
            queue_insert_tail(&g_mem_ctx.blks[i].usable, &obj->queue);
            p += (sizeof(mem_obj_t) + obj_space);
        }
    }
    return 0;
}

int mem_pool_free(void * p)
{
    pthread_mutex_lock(&g_mp_thread_lock);
    mem_obj_t * obj = ptr_get_struct(p, mem_obj_t, addr);
    schk(obj->magic == MP_OBJ_MAGIC, return -1);
    queue_remove(&obj->queue);
    queue_insert_tail(&g_mem_ctx.blks[obj->blk_idx].usable, &obj->queue);
    pthread_mutex_unlock(&g_mp_thread_lock);
    return 0;
}

void * mem_pool_alloc(int size)
{
    int blk_idx = 0;
    int blk_size = 0;
    assert(size > 0);

    pthread_mutex_lock(&g_mp_thread_lock);
    ///choice block
    if(size <= 512) {
        blk_idx = 0;
        blk_size = 512;
    } else if (size > 512 && size <= 1024) {
        blk_idx = 1;
        blk_size = 1024;
    } else if (size > 1024 && size <= 2048) {
        blk_idx = 2;
        blk_size = 2048;
    } else if (size > 2048 && size <= 4096) {
        blk_idx = 3;
        blk_size = 4096;
    } else if (size > 4096 && size <= 8192) {
        blk_idx = 4;
        blk_size = 8192;
    } else if (size > 8192 && size <= 16384) {
        blk_idx = 5;
        blk_size = 16384;
    } else {
        err("alloc size [%d] too big. not support\n", size);
        pthread_mutex_unlock(&g_mp_thread_lock);
        return NULL;
    }
    schk(!queue_empty(&g_mem_ctx.blks[blk_idx].usable), {pthread_mutex_unlock(&g_mp_thread_lock);return NULL;});
   
    queue_t * q = queue_head(&g_mem_ctx.blks[blk_idx].usable);
    mem_obj_t * obj = ptr_get_struct(q, mem_obj_t, queue);
    
    queue_remove(&obj->queue);
    queue_insert_tail(&g_mem_ctx.blks[blk_idx].inuse, &obj->queue);
    memset(obj->addr, 0x0, blk_size);
    pthread_mutex_unlock(&g_mp_thread_lock);
    return obj->addr;
}

char * mem_pool_ver() 
{
    return g_mp_ver;
}
#endif


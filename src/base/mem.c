#include "common.h"

static pthread_mutex_t mem_th_lock = PTHREAD_MUTEX_INITIALIZER;

#define MEM_POOL "MEM_POOL"
#define VER	"0.1"
#define MEM_POOL_BLOCKN     6
#define MEM_POOL_OBJN       1024


typedef struct
{
    int fuse;
    char addr[0];
} mem_obj_t;

typedef struct
{
    mem_obj_t ** list;
    char * listaddr;
    int listaddrn;
} mem_block_t;

typedef struct 
{
	mem_block_t * arr;
    /*
        0:512   byte
        1:1024  
        2:2048
        3:4096
        4:8192
        5:16384
    */
} mem_ctx_t;

static mem_ctx_t g_mem_ctx;


void * sys_alloc( int size )
{
    assert( size > 0 );
    pthread_mutex_lock( &mem_th_lock );
    char * addr = calloc( 1, size );
    pthread_mutex_unlock( &mem_th_lock );
    if( !addr ) {
        err("sys alloc failed. [%d]\n", errno );
        return NULL;
    }
    return addr;
}

void sys_free( void * addr )
{
    if( addr ) {
        pthread_mutex_lock( &mem_th_lock );
        free(addr);
        pthread_mutex_unlock( &mem_th_lock );
    }
}

int mem_pool_deinit()
{
	int i = 0;
    if( g_mem_ctx.arr ) {
        for( i = 0; i < MEM_POOL_BLOCKN; i ++ ) {
            if( g_mem_ctx.arr[i].listaddr ) {
        		free(g_mem_ctx.arr[i].listaddr);
            }
            if( g_mem_ctx.arr[i].list ) {
        		free(g_mem_ctx.arr[i].list);
            }
    	}
    	free(g_mem_ctx.arr);
    }
	return 0;
}

int mem_pool_init()
{
	int i = 0;
	int j = 0;

	g_mem_ctx.arr = malloc( sizeof(mem_block_t)*MEM_POOL_BLOCKN );
	if( !g_mem_ctx.arr ) {
		err("malloc blocks failed.\n");
		return -1;
    }
    memset( g_mem_ctx.arr, 0x0, sizeof(mem_block_t)*MEM_POOL_BLOCKN );
    

	for( i = 0; i < MEM_POOL_BLOCKN; i ++) {
        
        int obj_space = 0;
        int obj_n = MEM_POOL_OBJN;
    
		if(i == 0) {
            obj_space = 512;   
            obj_n = 2 * MEM_POOL_OBJN; /// small obj need more
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
        
		///
		g_mem_ctx.arr[i].list = malloc( sizeof(mem_obj_t*)*obj_n );
		if( !g_mem_ctx.arr[i].list ) {
			err("malloc idx [%d] list point failed\n", i );
            mem_pool_deinit();
			return -1;
		}
		memset( g_mem_ctx.arr[i].list, 0x0, sizeof(mem_obj_t*)*obj_n );

        /// 
		g_mem_ctx.arr[i].listaddrn = (sizeof(mem_obj_t)+obj_space)*obj_n;
		g_mem_ctx.arr[i].listaddr = malloc( g_mem_ctx.arr[i].listaddrn );
		if(!g_mem_ctx.arr[i].listaddr) {
			err("malloc idx [%d] list failed\n", i);
            mem_pool_deinit();
			return -1;
		}
		memset( g_mem_ctx.arr[i].listaddr, 0x0, g_mem_ctx.arr[i].listaddrn );
        
		///		
		char * p = g_mem_ctx.arr[i].listaddr;
		for( j = 0; j < obj_n; j ++) {
			g_mem_ctx.arr[i].list[j] = (mem_obj_t*)p;
			g_mem_ctx.arr[i].list[j]->fuse = 0;

			p += sizeof(mem_obj_t)+obj_space;
		}
	}
	return 0;
}

int mem_pool_free( void * addr)
{
    ///getting mem_obj_t by address offset
	mem_obj_t * obj = (mem_obj_t*)(addr-sizeof(mem_obj_t));
    if( obj->fuse ) {
        obj->fuse = 0;  ///reset the flag. so that can alloc again
    }
	return 0;
}

void * mem_pool_alloc(int size)
{
    int obj_block = 0;
    int obj_size = 0;
    assert(size > 0);

    
    /// find which block
    if( size <= 512 ) {
        obj_block = 0;
        obj_size = 512;
    } else if ( size > 512 && size <= 1024 ) {
        obj_block = 1;
        obj_size = 1024;
    } else if ( size > 1024 && size <= 2048 ) {
        obj_block = 2;
        obj_size = 2048;
    } else if ( size > 2048 && size <= 4096 ) {
        obj_block = 3;
        obj_size = 4096;
    } else if ( size > 4096 && size <= 8192 ) {
        obj_block = 4;
        obj_size = 8192;
    } else if ( size > 8192 && size <= 16384 ) {
        obj_block = 5;
        obj_size = 16384;
    } else {
        err("alloc size [%d] too big. not support\n", size );
        return NULL;
    }

    int j = 0;
    for( j = 0; j < MEM_POOL_OBJN; j ++ ) {
        if(!g_mem_ctx.arr[obj_block].list[j]->fuse) {
            g_mem_ctx.arr[obj_block].list[j]->fuse = 1;
            memset( g_mem_ctx.arr[obj_block].list[j]->addr, 0x0, obj_size ); ///clear dirty data for convenicence 
            return g_mem_ctx.arr[obj_block].list[j]->addr;
        }
    }
    err("mem pool alloc size [%d] failed.\n", size );
    return NULL;
}



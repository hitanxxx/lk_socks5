#include "common.h"
#include "modules.h"

#define				L_PROCESS_MASTER		0xff

typedef struct process_ctx
{
	sys_shm_t	shm;
	
	int		    id;
	process_t* 	arr;
	int 		*lock;
#if __linux__
	sem_t		*sem;
#elif __APPLE__
    dispatch_semaphore_t  sem;
#endif
	

	// signal values
	int32				signal;
	sig_atomic_t		sig_quit;
	sig_atomic_t		sig_reap;
	sig_atomic_t		sig_reload;
} process_ctx_t;

static process_ctx_t * g_proc_ctx = NULL;


status proc_pid( )
{
	return g_proc_ctx->arr[g_proc_ctx->id].pid;
}

status proc_pid_form_file( pid_t * pid )
{
	int32 fd=  0;
	char str[128] = {0};

	fd = open( L_PATH_PIDFILE, O_RDONLY );
	if( ERROR == fd ) 
	{
		err("open pid file\n" );
		return ERROR;
	}
	if( ERROR == read( fd, str, sizeof(str) ) ) 
	{
		err("read pid file\n" );
		close( fd );
		return ERROR;
	}
    *pid = strtol( str, NULL, 10 );
	return OK;
}

status proc_signal_send( pid_t pid, int32 signal )
{
	if( ERROR == kill( pid, signal ) ) {
		err("send sginal [%d] to pid [%d] failed, [%d]\n", signal, pid, errno );
		return ERROR;
	}
	return OK;
}

static status proc_signal_bcast( int32 lsignal )
{
	int i = 0;

	for( i = 0; i < config_get()->sys_process_num; i ++ ) {
		if( ERROR == proc_signal_send( g_proc_ctx->arr[i].pid, lsignal ) ) {
			err("broadcast signal failed\n");
			return ERROR;
		}
	}
	return OK;
}

void proc_worker_run( void )
{
	int32 timer;
	sigset_t set;

	/// worker process clear a signal set
	sigemptyset( &set );
	/// worker process set the empty signal set to block
	/// it is equal to not block any signal
	sigprocmask( SIG_SETMASK, &set, NULL );

	/// init process modules
	modules_process_init();
	while( 1 )  {
		if( g_proc_ctx->sig_quit )  {
			g_proc_ctx->sig_quit = 0;
			break;
		}
		timer_expire( &timer );
		event_run( timer );
	}
	modules_pocess_exit();
}

static status proc_fork( process_t * process )
{
	pid_t pid = fork( );
	if( pid < 0 ) {
		err("fork child failed, [%d]\n", errno );
		return ERROR;
	} else if ( pid == 0 ) {
        g_proc_ctx->id = process->sequence_num;
        proc_worker_run();
	} else if ( pid > 0 ) {
	    process->pid = pid;
	}
	return OK;
}

void proc_master_run( void )
{
	int i;
	sigset_t set;

	/// blocking this kind of signals
	/// which signal will be reprocess when blocking is remove
    sigemptyset( &set );
    sigaddset( &set, SIGCHLD );
    sigaddset( &set, SIGINT );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );
    if( sigprocmask( SIG_BLOCK, &set, NULL ) == ERROR ) {
		err("master blcok signal set failed, [%d]\n", errno );
		return;
    }

	/// start worker process
	for( i = 0; i < config_get()->sys_process_num; i ++ ) {
	    if( ERROR == proc_fork( &g_proc_ctx->arr[i] ) ) {
            err("process spawn number [%d] child failed, errno [%d]\n", i, errno );
			return;
	    }

		if( g_proc_ctx->id != L_PROCESS_MASTER ) {
		    return;
		}   
	}
	
	// clear signal set
	sigemptyset(&set);
	while( 1 ) {
		/// sigsuspend is a automic option
		/// 1. clear all block signal
		/// 2. wait a signal recvied 
		/// and when it return. will recovery signal blcok to before it called
		sigsuspend( &set );
		systime_update( );
		err("master received signal [%d]\n", g_proc_ctx->signal );

        if( g_proc_ctx->sig_quit == 1 ) {
            proc_signal_bcast( SIGINT );
            int alive = 0;
            for( i = 0; i < config_get()->sys_process_num; i ++ ) {
                if( g_proc_ctx->arr[i].exited != 1 ) {
                    alive ++;
                }
            }
            if( alive == 0 ) {
                break;
            }
        }

        if( g_proc_ctx->sig_reap == 1 ) {
            for( i = 0; i < config_get()->sys_process_num; i ++ ) {
                if( g_proc_ctx->arr[i].exited == 1 ) {
                    /// clear child event mutex value
                    process_lock();
                	if( process_mutex_value_get() == g_proc_ctx->arr[i].pid ) {
                		process_mutex_value_set(0);
                	}
                	process_unlock();
                    
                
                    if( g_proc_ctx->sig_quit != 1 ) {
                        if( ERROR == proc_fork( &g_proc_ctx->arr[i] ) ) {
        					err("proc_fork index [%d] failed, [%d]\n", i, errno );
        					continue;
        				}

        				if( g_proc_ctx->id != L_PROCESS_MASTER ) {
                		    return;   
        				} else {
        				    g_proc_ctx->arr[i].exited = 0;
        				}
                    }
                }
            }
            g_proc_ctx->sig_reap = 0;
        }
        
        if( g_proc_ctx->sig_reload == 1 ) {
            proc_signal_bcast( SIGINT );
            g_proc_ctx->sig_reload = 0;
        }
	}
}

void proc_waitpid( )
{
	int32 i; 

	while(1) {
	    /// wait to get anyone child dead pid and no block
		pid_t dead_child_pid = waitpid( -1, NULL, WNOHANG );
		if( dead_child_pid == 0 ) {
		    /// no any child dead. (some error happen)
		    return;
		} else if( dead_child_pid == -1 ) {
            if( errno == EINTR ) {
                /// interrupt by signal, continue
                continue;
            }
            return;
		}

		for( i = 0; i < config_get()->sys_process_num; i ++ ) {
 			if( dead_child_pid == g_proc_ctx->arr[i].pid ) {
				g_proc_ctx->arr[i].exited = 1;
				break;
			}
		}
	}
	return;
}

void proc_signal_cb( int32 signal )
{
    /// record sys errno 
	int errno_cache = errno;
	
	g_proc_ctx->signal = signal;

	/// master process deal with signal
	if( g_proc_ctx->id == L_PROCESS_MASTER ) {
		if( signal == SIGINT ) {
			g_proc_ctx->sig_quit = 1;
		} else if( signal == SIGCHLD ) {
			g_proc_ctx->sig_reap = 1;
			proc_waitpid();
		} else if ( signal == SIGHUP ) {
			g_proc_ctx->sig_reload = 1;
		}
	} else {
		/// worker process
		if( signal == SIGINT )
			g_proc_ctx->sig_quit = 1;
	}
	
	/// recovery sys errno
	errno = errno_cache;
}

status proc_signal_init()
{
	uint32 i;
	struct sigaction sa;
	// all signal use one callback function
	int32 sig_arr[] = {
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGPIPE,
		SIGUSR1,
		SIGUSR2,
		0
	};

	for( i = 0; sig_arr[i]; i ++ )  {
		memset( &sa, 0, sizeof(struct sigaction) );
		sigemptyset( &sa.sa_mask );
		sa.sa_handler 	= proc_signal_cb;
		sa.sa_flags 	= SA_SIGINFO;
		if( sigaction( sig_arr[i], &sa, NULL ) == -1 ) {
			err("set sys signal [%d] callback failed, errno [%d]\n", sig_arr[i], errno );
			return ERROR;
		}
	}
	return OK;
}

void process_lock( )
{	
#if __linux__
    int ret = 0;
    do {
        ret = sem_wait( g_proc_ctx->sem );
    } while( ret == -1 );
#elif __APPLE__
    dispatch_semaphore_wait( g_proc_ctx->sem, DISPATCH_TIME_FOREVER);
#endif
}

void process_unlock( )
{
#if __linux__
	sem_post( g_proc_ctx->sem );
#elif __APPLE__
    dispatch_semaphore_signal(g_proc_ctx->sem);
#endif
}


void  process_mutex_value_set( int v )
{
	*g_proc_ctx->lock = v;
}

int process_mutex_value_get( ) 
{
	return *g_proc_ctx->lock;
}

status process_init( void )
{
	uint32 sequence = 0;

    if( g_proc_ctx ) {
		err("process ctx not empty\n");
		return -1;
	}

	/// register all process signal callback
	if( OK != proc_signal_init() ) {
		err("signal init failed\n");
		return ERROR;
	}

	g_proc_ctx = l_safe_malloc( sizeof(process_ctx_t) );
	if( !g_proc_ctx ) {
		err("alloc process ctx failed, [%d]\n", errno );
		return -1;
	}
	memset( g_proc_ctx, 0, sizeof(process_ctx_t) );
	g_proc_ctx->id = L_PROCESS_MASTER;

	/// init shm 
    g_proc_ctx->shm.size += (config_get()->sys_process_num * sizeof(process_t) );
    g_proc_ctx->shm.size += sizeof(int);
#if __linux__
    g_proc_ctx->shm.size += sizeof(sem_t);
#endif
	if( OK != sys_shm_alloc( &g_proc_ctx->shm, g_proc_ctx->shm.size ) ) {
		err("process shm memory alloc failed\n" );
		return ERROR;
	}

	g_proc_ctx->arr = (process_t*)g_proc_ctx->shm.data;
	for( sequence = 0; sequence < config_get()->sys_process_num; sequence ++ )
		g_proc_ctx->arr[sequence].sequence_num = sequence;
	g_proc_ctx->lock = (int*)( g_proc_ctx->shm.data + ( sizeof(process_t)*config_get()->sys_process_num ) );
	*g_proc_ctx->lock = 0;

#if __linux__
	// build process lock
	g_proc_ctx->sem = (sem_t*)(g_proc_ctx->shm.data +( sizeof(process_t)*config_get()->sys_process_num ) + sizeof(int) );
	if( OK != sem_init( g_proc_ctx->sem, 1, 1 ) ) {
		err("sem_init failed, [%d]\n", errno );
		return ERROR;
	}
#elif __APPLE__
    g_proc_ctx->sem = dispatch_semaphore_create( 1 );
    if( g_proc_ctx->sem == NULL ) {
        err("dispatch_semaphore_create failed. [%d]\n", errno );
        return ERROR;
    }
#endif
	return OK;
}

status process_end( void )
{
	if( g_proc_ctx ) {
		if( g_proc_ctx->shm.size > 0 && g_proc_ctx->shm.data ) {
#if __linux__
			if( g_proc_ctx->sem ) {
				if( OK != sem_destroy(g_proc_ctx->sem) ) {
					err("sem_destory failed, [%d]\n", errno );
				}
			}
#endif	
			sys_shm_free( &g_proc_ctx->shm );
			g_proc_ctx->shm.size = 0;
		}
		l_safe_free(g_proc_ctx);
	}
	return OK;
}

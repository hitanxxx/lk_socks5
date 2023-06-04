#include "common.h"
#include "modules.h"

#define				L_PROCESS_MASTER		0xff

typedef struct 
{
	sys_shm_t		shm;
	
	uint32		id;
	process_t* 	arr;
	uint32		all;
#if __linux__
	sem_t		*sem;
#elif __APPLE__
    dispatch_semaphore_t   sem;
#endif
	int 		*lock;

	// signal values
	int32				signal;
	sig_atomic_t		sig_quit;
	sig_atomic_t		sig_reap;
	sig_atomic_t		sig_reload;
} g_process_t;

static g_process_t * g_proc_ctx = NULL;


static int32 proc_reap( void );
static pid_t proc_spawn( process_t * data );


status proc_self_pid( pid_t * pid )
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

status proc_send_signal( pid_t pid, int32 signal )
{
	if( ERROR == kill( pid, signal ) ) 
	{
		err("send sginal [%d] to pid [%d] failed, [%d]\n", signal, pid, errno );
		return ERROR;
	}
	return OK;
}

static status proc_send_signal_childs( int32 lsignal )
{
	uint32 i = 0;

	for( i = 0; i < g_proc_ctx->all; i ++ ) 
	{
		if( ERROR == proc_send_signal( g_proc_ctx->arr[i].pid, lsignal ) ) 
		{
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

	/// worker process not block any signal
	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );

	/// init process modules
	modules_process_init();
	while( 1 ) 
	{
		if( g_proc_ctx->sig_quit ) 
		{
			g_proc_ctx->sig_quit = 0;
			break;
		}
		timer_expire( &timer );
		event_run( timer );
	}
	modules_pocess_exit();
}

static int32 proc_reap( void )
{
	// return child live num
	status live = 0;
	uint32 i;

	for( i = 0; i < g_proc_ctx->all; i ++ ) 
	{
		if( g_proc_ctx->arr[i].exited == 1 ) 
		{
			// if not quit cmd, start new work process
			if( 1 != g_proc_ctx->sig_quit ) 
			{
				if( ERROR == proc_spawn( &g_proc_ctx->arr[i] ) ) 
				{
					err("proc_spawn index [%d] failed, [%d]\n", i, errno );
					continue;
				}
				g_proc_ctx->arr[i].exited = 0;
				live += 1;
			}
		} 
		else 
		{
			live += 1;
		}
	}
	return live;
}

static pid_t proc_spawn( process_t * data )
{
	pid_t pid;

	pid = fork( );
	if( pid < 0 ) 
	{
		err("fork child failed, [%d]\n", errno );
		return ERROR;
	} 
	else if ( pid == 0 ) /// child
		g_proc_ctx->id = data->sequence_num;
	else if ( pid > 0 )  /// parent
		data->pid = pid;
	return OK;
}

void proc_master_run( void )
{
	int32 live = 1;
	int32 i;
	status ret;
	sigset_t set;

	/// block the signals
    sigemptyset( &set );
    sigaddset( &set, SIGCHLD );
    sigaddset( &set, SIGINT );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );
    if( sigprocmask( SIG_BLOCK, &set, NULL ) == ERROR )
	{
		err("master blcok signal failed, [%d]\n", errno );
		return;
    }

	/// start worker process
	for( i = 0; i < g_proc_ctx->all; i ++ )
	{
		ret = proc_spawn( &g_proc_ctx->arr[i] );
		if( ret == ERROR )
		{
			err("process spawn number [%d] child failed, errno [%d]\n", i, errno );
			return;
		}

		/// child jump the loop
		if( g_proc_ctx->id != L_PROCESS_MASTER ) break;
	}
	/// child goto got work
	if( g_proc_ctx->id != L_PROCESS_MASTER )
	{
		proc_worker_run(); 
		/// if child break the worker run loop. means need to stop
		return;
	} 
	
	// clear signal set
	sigemptyset(&set);
	while( 1 ) 
	{
		/*
			set block signal set to empty
			clear all block signal
		*/
		sigsuspend( &set );
		systime_update( );
		err("master received signal [%d]\n", g_proc_ctx->signal );

		if( g_proc_ctx->sig_reap ) 
		{
			g_proc_ctx->sig_reap = 0;
			live = proc_reap(  );
		}
        
		if( !live && g_proc_ctx->sig_quit ) 
			break;
        
		if( g_proc_ctx->sig_quit == 1 || g_proc_ctx->sig_reload == 1 ) 
		{
			if( g_proc_ctx->sig_reload == 1 )
				g_proc_ctx->sig_reload = 0;
            
			proc_send_signal_childs( SIGINT );
			continue;
		}
	}
}

void proc_child_status( )
{
	int32 pid;
	int32 i; 

	while(1)
	{
		pid = waitpid( -1, NULL, WNOHANG );
		if( pid == 0 ) // no child dead;
			return;
		if( pid == -1 )
		{
			if( errno == EINTR ) /// stop by signal
				continue;
			return; /// error
		}

		/// find the child pid and changed status
		for( i = 0; i < g_proc_ctx->all; i ++ )
		{
 			if( pid == g_proc_ctx->arr[i].pid )
			{
				g_proc_ctx->arr[i].exited = 1;
				break;
			}
		}
	}
	return;
}

void proc_signal_cb( int32 signal )
{
	int errno_cache = errno;

	g_proc_ctx->signal = signal;

	/// master process deal with signal
	if( g_proc_ctx->id == L_PROCESS_MASTER )
	{
		if( signal == SIGINT )
			g_proc_ctx->sig_quit 	= 1;
		else if( signal == SIGCHLD )
			g_proc_ctx->sig_reap 	= 1;
		else if ( signal == SIGHUP )
			g_proc_ctx->sig_reload	= 1;

		if( signal == SIGCHLD ) proc_child_status();
	}
	else
	{
		/// worker process
		if( signal == SIGINT )
			g_proc_ctx->sig_quit = 1;
	}
	errno = errno_cache;
}

status proc_signal_init()
{
	uint32 i;
	struct sigaction sa;
	// all signal use one callback function
	int32 sig_arr[] = 
	{
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGPIPE,
		SIGUSR1,
		SIGUSR2,
		0
	};

	for( i = 0; sig_arr[i]; i ++ ) 
	{
		memset( &sa, 0, sizeof(struct sigaction) );
		sigemptyset( &sa.sa_mask );
		sa.sa_handler 	= proc_signal_cb;
		sa.sa_flags 	= SA_SIGINFO;
		if( sigaction( sig_arr[i], &sa, NULL ) == -1 ) 
		{
			err("set ORsignal [%d] handler failed, errno [%d]\n", sig_arr[i], errno );
			return ERROR;
		}
	}
	return OK;
}

void process_lock( )
{	
#if __linux__
    int ret = 0;
    do 
    {
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

status process_get_pid( )
{
	return g_proc_ctx->arr[g_proc_ctx->id].pid;
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

	/// register all process signal callback
	if( OK != proc_signal_init() )
	{
		err("signal env init failed\n");
		return ERROR;
	}

	if( g_proc_ctx )
	{
		err("process ctx not empty\n");
		return -1;
	}
	g_proc_ctx = l_safe_malloc( sizeof(g_process_t) );
	if( !g_proc_ctx )
	{
		err("alloc process ctx failed, [%d]\n", errno );
		return -1;
	}
	memset( g_proc_ctx, 0, sizeof(g_process_t) );
	g_proc_ctx->id = L_PROCESS_MASTER;
	
	// create share memouy
	// 1, every child know other's pid
	// 2, sem lock 
	g_proc_ctx->all = config_get()->sys_process;
    g_proc_ctx->shm.size += sizeof(process_t) * MAXPROCESS;
#if __linux__
    g_proc_ctx->shm.size += sizeof(sem_t);
#endif
    g_proc_ctx->shm.size += sizeof(int);
	if( OK != sys_shm_alloc( &g_proc_ctx->shm, g_proc_ctx->shm.size ) ) 
	{
		err("process shm memory alloc failed\n" );
		return ERROR;
	}

	g_proc_ctx->arr = (process_t*)g_proc_ctx->shm.data;
	for( sequence = 0; sequence < g_proc_ctx->all; sequence ++ )
	{
		g_proc_ctx->arr[sequence].sequence_num = sequence;
	}

#if __linux__
	// build process lock
	g_proc_ctx->sem = (sem_t*)(g_proc_ctx->shm.data +( sizeof(process_t)*MAXPROCESS ));
	if( OK != sem_init( g_proc_ctx->sem, 1, 1 ) )
	{
		err("sem_init failed, [%d]\n", errno );
		return ERROR;
	}
	// build mutex value
	g_proc_ctx->lock = (int*)( g_proc_ctx->shm.data +( sizeof(process_t)*MAXPROCESS ) +sizeof(sem_t) );
    
#elif __APPLE__
    g_proc_ctx->sem = dispatch_semaphore_create( 1 );
    if( g_proc_ctx->sem == NULL )
    {
        err("dispatch_semaphore_create failed. [%d]\n", errno );
        return ERROR;
    }
    g_proc_ctx->lock = (int*) ( g_proc_ctx->shm.data + (sizeof(process_t)*MAXPROCESS) );
#endif
    *g_proc_ctx->lock = 0;
	
	return OK;
}

status process_end( void )
{
	if( g_proc_ctx )
	{
		if( g_proc_ctx->shm.size > 0 && g_proc_ctx->shm.data )
		{
#if __linux__
			if( g_proc_ctx->sem )
			{
				if( OK != sem_destroy(g_proc_ctx->sem) )
				{
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

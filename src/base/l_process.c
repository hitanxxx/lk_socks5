#include "l_base.h"
#include "l_module.h"

#define				L_PROCESS_MASTER		0xff

static l_shm_t		g_shm_process;
static process_t* 	g_process_arr;
static uint32		g_process_num = 0;
static uint32		g_process_id = L_PROCESS_MASTER;

// signal values
static	int32		g_signal = 0;
sig_atomic_t		sig_quit = 0;
sig_atomic_t		sig_reap = 0;
sig_atomic_t		sig_reload = 0;

static int32 process_reap( void );
static pid_t process_spawn( process_t * data );



status process_self_pid( pid_t * pid )
{
	int32 fd, localpid;
	char str[128] = {0};

	fd = open( L_PATH_PIDFILE, O_RDONLY );
	if( ERROR == fd ) {
		err(" open pid file\n" );
		return ERROR;
	}
	if( ERROR == read( fd, str, sizeof(str) ) ) {
		err(" read pid file\n" );
		close( fd );
		return ERROR;
	}
	if( OK != l_atoi( str, l_strlen(str), &localpid ) ) {
		err(" atoi pid file value [%s]\n", str );
		return ERROR;
	}
	*pid = localpid;
	return OK;
}

status process_send_signal( pid_t pid, int32 signal )
{
	if( ERROR == kill( pid, signal ) ) 
	{
		err("send sginal [%d] to pid [%d] failed, [%d]\n", signal, pid, errno );
		return ERROR;
	}
	return OK;
}

static status process_send_signal_childs( int32 lsignal )
{
	uint32 i = 0;

	for( i = 0; i < g_process_num; i ++ ) 
	{
		if( ERROR == process_send_signal( g_process_arr[i].pid, lsignal ) ) 
		{
			err(" broadcast signal failed, [%d]\n" );
			return ERROR;
		}
	}
	return OK;
}

static void process_worker_run( void )
{
	int32 timer;
	sigset_t set;

	// not block any signal
	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );
	
	event_init();
	while( 1 ) 
	{
		if( sig_quit ) 
		{
			sig_quit = 0;
			event_end();
			modules_end();
			exit(0);
		}
		timer_expire( &timer );
		event_run( timer );
	}
}

static int32 process_reap( void )
{
	status live = 0;
	uint32 i;

	for( i = 0; i < g_process_num; i ++ ) 
	{
		if( g_process_arr[i].exited == 1 ) 
		{
			if( !sig_quit ) 
			{
				if( ERROR == process_spawn( &g_process_arr[i] ) ) 
				{
					err(" process_spawn index [%d] failed, [%d]\n", i, errno );
					continue;
				}
				g_process_arr[i].exited = 0;
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

static pid_t process_spawn( process_t * data )
{
	pid_t pid;

	pid = fork( );
	if( pid < 0 ) 
	{
		err(" fork child failed, [%d]\n", errno );
		return ERROR;
	} 
	else if ( pid == 0 )
	{
		// child 
		g_process_id = data->sequence_num;
		process_worker_run();
	}
	else if ( pid > 0 )
	{
		data->pid = pid;
	}
	return OK;
}


status process_worker_start( )
{
	int32 index;
	pid_t pid;

	for( index = 0; index < g_process_num; index ++ )
	{
		pid = process_spawn( &g_process_arr[index] );
		if( pid == ERROR )
		{
			err("process spawn number [%d] child failed, errno [%d]\n", index, errno );
			return ERROR;
		}
	}
	return OK;
}


void process_master_run( void )
{
	uint32 i = 0;
	int32 live = 1;
	process_t * process;
	sigset_t set;

	// block some signal
    sigemptyset( &set );
    sigaddset( &set, SIGCHLD );
    sigaddset( &set, SIGINT );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );
    if( sigprocmask( SIG_BLOCK, &set, NULL ) == ERROR ) 
	{
		err(" master blcok signal failed, [%d]\n", errno );
		return;
    }

	if( OK != process_worker_start() )
	{
		return;
	}
	
	// waitting signal
	sigemptyset(&set);
	while( 1 ) 
	{
		sigsuspend( &set );
		l_time_update( );
		debug(" master received signal [%d]\n", g_signal);

		if( sig_reap ) 
		{
			sig_reap = 0;
			live = process_reap(  );
		}
		if( !live && sig_quit ) 
		{
			break;
		}
		if( sig_quit == 1 || sig_reload == 1 ) 
		{
			if( sig_reload == 1 )
			{
				sig_reload = 0;
			}
			process_send_signal_childs( SIGINT );
			continue;
		}
	}
		
}

void process_single_run( void )
{
	int32 timer;
	sigset_t set;

	// not blcok any signal
	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );
	
	event_init();
	while( 1 ) 
	{
		if( sig_quit ) 
		{
			sig_quit = 0;
			event_end();
			break;
		}
		timer_expire( &timer );
		event_run( timer );
	}
}

void process_child_status( )
{
	int32 pid;
	int32 index; 

	while(1)
	{
		pid = waitpid( -1, NULL, WNOHANG );
		if( pid == 0 )
		{
			// no child dead;
			return;
		}
		if( pid == -1 )
		{
			if( errno == EINTR )
			{
				// stop by signal
				continue;
			}
			// all child dead
			return;
		}
		for( index = 0; index < g_process_num; index ++ )
		{
			if( pid == g_process_arr[index].pid )
			{
				g_process_arr[index].exited = 1;
				break;
			}
		}
	}
	return;
}

void signal_handler( int32 signal )
{
	int errno_cache = errno;

	g_signal = signal;

	if( g_process_id == L_PROCESS_MASTER )
	{
		if( signal == SIGINT )
		{
			sig_quit 	= 1;
		}
		else if( signal == SIGCHLD )
		{
			sig_reap 	= 1;
		}
		else if ( signal == SIGHUP )
		{
			sig_reload	= 1;
		}
	}
	else
	{
		if( signal == SIGINT )
		{
			sig_quit = 1;
		}
	}
	// only master have child
	if( signal == SIGCHLD )
	{
		process_child_status();
	}
	errno = errno_cache;
}

status signal_env_init()
{
	uint32 i;
	struct sigaction sa;
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
		sa.sa_handler 	= signal_handler;
		sa.sa_flags 	= SA_SIGINFO;
		if( sigaction( sig_arr[i], &sa, NULL ) == -1 ) 
		{
			err(" set signal [%d] handler failed, errno [%d]\n", sig_arr[i], errno );
			return ERROR;
		}
	}
	return OK;
}

status process_init( void )
{
	uint32 index;

	if( OK != signal_env_init() )
	{
		err("signal env init failed\n");
		return ERROR;
	}

	// create share memouy, so every child know other's pid
	memset( &g_shm_process, 0, sizeof(l_shm_t) );
	g_shm_process.size = sizeof(process_t)*MAXPROCESS;
	if( OK != l_shm_alloc( &g_shm_process, g_shm_process.size ) ) 
	{
		err(" l_shm_alloc process failed\n" );
		return ERROR;
	}
	g_process_arr = (process_t*)g_shm_process.data;

	g_process_num = conf.base.worker_process;
	for( index = 0; index < g_process_num; index ++ ) 
	{
		g_process_arr[index].sequence_num = index;
	}
	return OK;
}

status process_end( void )
{
	if( g_shm_process.size ) 
	{
		l_shm_free( &g_shm_process );
	}
	g_shm_process.size = 0;
	return OK;
}

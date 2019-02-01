#include "lk.h"

l_shm_t		shm_process;
process_t* 	process_arr;
uint32		process_num = 0;
uint32		process_id = 0xffff;

// process_get_run --------------
process_t * process_get_run( void )
{
	if( process_num < 1 ) {
		return NULL;
	}
	return &process_arr[process_id];
}
// process_broadcast_child -----------------
static status process_broadcast_child( int32 lsignal )
{
	uint32 i = 0;

	for( i = 0; i < process_num; i ++ ) {
		if( ERROR == kill( process_arr[i].pid, lsignal ) ) {
			err_log( "%s --- broadcast signal [%d] failed, [%d]", __func__,
			lsignal, errno );
		}
		process_arr[i].exiting = 1;
	}
	return OK;
}
// process_worker_end ------------------
static void process_worker_end( void )
{
	err_log( "%s --- worker process exiting", __func__ );
	dynamic_module_end( );
	modules_end();
	exit(0);
}
// process_worker_run -------------------
static void process_worker_run( void )
{
	int32 timer;
	sigset_t set;

	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );
	dynamic_module_init( );
	while( 1 ) {
		if( sig_quit ) {
			sig_quit = 0;
			process_worker_end( );
		}
		timer_expire( &timer );
		event_loop( timer );
	}
}
// process_spawn ------------------
static pid_t process_spawn( process_t * pro )
{
	pid_t id;

	id = fork( );
	if( id < 0 ) {
		err_log( "%s --- fork failed, [%d]", __func__, errno );
		return ERROR;
	} else if ( id > 0 ) {
		// parent
		pro->pid = id;
	} else if ( id == 0 ) {
		// child
		process_id = pro->nid;
		process_worker_run( );
	}
	return id;
}
// process_reap ----------------------
static int32 process_reap( void )
{
	status live = 0;
	uint32 i;

	for( i = 0; i < process_num; i ++ ) {

		if( process_arr[i].exited ) {
			if( !process_arr[i].exiting && !sig_quit ) {
				if( ERROR == process_spawn( &process_arr[i] ) ) {
					err_log( "%s --- process_spawn failed, [%d]", __func__, errno );
					continue;
				}
				process_arr[i].exited = 0;
				live = 1;
			}
		} else {
			if( process_arr[i].exiting ) {
				live = 1;
			}
		}
	}
	return live;
}
// process_master_run -------------------------
void process_master_run( void )
{
	uint32 i = 0;
	int32 live = 1;
	sigset_t set;

    sigemptyset( &set );
    sigaddset( &set, SIGCHLD );
    sigaddset( &set, SIGINT );
	sigaddset( &set, SIGUSR1 );
	sigaddset( &set, SIGUSR2 );
    if( sigprocmask( SIG_BLOCK, &set, NULL ) == ERROR ) {
		err_log( "%s --- sigs_suppend_init failed, [%d]", __func__, errno );
		return;
    }
	sigemptyset(&set);

	// fork child
	for( i = 0; i < process_num; i++ ) {
		if( ERROR == process_spawn( &process_arr[i] ) ) {
			err_log( "%s --- process spawn failed, [%d]", __func__, errno );
			return;
		}
	}
	// goto loop wait signal
	while( 1 ) {
		sigsuspend( &set );
		l_time_update( );
		debug_log("%s --- master received signal [%d]", __func__, global_signal);

		if( sig_reap ) {
			sig_reap = 0;
			live = process_reap(  );
			err_log("%s --- reap child", __func__ );
		}
		if( !live && sig_quit ) {
			break;
		}
		if( sig_quit ) {
			process_broadcast_child( SIGINT );
			continue;
		}
		if( sig_reload ) {
			sig_reload = 0;
			process_broadcast_child( SIGINT );
			continue;
		}
	}
}
// process_single_run ---------------
void process_single_run( void )
{
	int32 timer;
	sigset_t set;

	sigemptyset( &set );
	sigprocmask( SIG_SETMASK, &set, NULL );
	dynamic_module_init( );
	while( 1 ) {
		if( sig_quit ) {
			sig_quit = 0;
			dynamic_module_end();
			break;
		}
		timer_expire( &timer );
		event_loop( timer );
	}
}
// process_init -----------------------
status process_init( void )
{
	uint32 i;

	memset( &shm_process, 0, sizeof(l_shm_t) );
	shm_process.size = sizeof(process_t)*MAXPROCESS;
	if( OK != l_shm_alloc( &shm_process, shm_process.size ) ) {
		err_log("%s --- l_shm_alloc failed", __func__ );
		return ERROR;
	}
	process_arr = (process_t*)shm_process.data;

	process_num = conf.worker_process;
	for( i = 0; i < process_num; i ++ ) {
		process_arr[i].nid = i;
	}
	return OK;
}
// process_end -----------------------
status process_end( void )
{
	if( shm_process.size ) {
		l_shm_free( &shm_process );
	}
	shm_process.size = 0;
	return OK;
}

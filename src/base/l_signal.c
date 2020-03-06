#include "l_base.h"

int32			global_signal = 0;
sig_atomic_t	sig_quit = 0;
sig_atomic_t	sig_reap = 0;
sig_atomic_t	sig_reload = 0;

// l_signal_child_status -------------------
static void l_signal_child_status( void )
{
	int32 pid;
	uint32 i;

	for( ;; ) {
		pid = waitpid( -1, NULL, WNOHANG );
		if( pid == 0 ) {
			// no child died...
			return;
		}
		if( pid == -1 ) {
			if( errno == EINTR ) {
				// stop by signal, continue
				continue;
			}
			return;
		}
		for( i = 0; i < process_num; i ++ ) {
			if( process_arr[i].pid == pid ) {
				process_arr[i].exiting = 0;
				process_arr[i].exited = 1;
				break;
			}
		}
	}
	return;
}
// l_signal_handler --------------------------
static void l_signal_handler( int32 lsignal )
{
	int32 errno_cache;
	/*
		signal handler don't change anything, and will be return immediately.
		so don't need any mutex lock.
	*/
	debug(" \n" );
	errno_cache = errno;
	global_signal = lsignal;
	if( process_id == 0xffff ) {
		if( lsignal == SIGINT ) {
			sig_quit = 1;
		} else if ( lsignal == SIGCHLD ) {
			sig_reap = 1;
		} else if ( lsignal == SIGHUP ) {
			sig_reload = 1;
		}
	} else {
		if( lsignal == SIGINT ) {
			sig_quit = 1;
		} 
	}

	if( lsignal == SIGCHLD ) {
		if( process_id == 0xffff ) {
			l_signal_child_status( );
		}
	}
	errno = errno_cache;
	return;
}
// l_signal_self -----------
status l_signal_self( int32 lsignal )
{
	int32 fd, pid;
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
	if( OK != l_atoi( str, l_strlen(str), &pid ) ) {
		err(" atoi pid file value [%s]\n", str );
		return ERROR;
	}
	if( ERROR == kill( pid, lsignal ) ) {
		err(" kill signal to pidfile, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}
// l_signal_init --------------------
status l_signal_init( void )
{
	uint32 i;
	struct sigaction sa;
	int32 sig_arr[] = {
		SIGINT,
		SIGHUP,
		SIGCHLD,
		SIGPIPE,
		SIGUSR1,
		SIGUSR2,
		0
	};

	for( i = 0; sig_arr[i]; i ++ ) {
		memset( &sa, 0, sizeof(struct sigaction) );
		sigemptyset( &sa.sa_mask );
		sa.sa_handler = l_signal_handler;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		if( sigaction( sig_arr[i], &sa, NULL ) == -1 ) {
			err(" set sig handler [%d]\n", sig_arr[i] );
			return ERROR;
		}
	}
	return OK;
}
// l_signal_end ---------------------
status l_signal_end( void )
{
	return OK;
}

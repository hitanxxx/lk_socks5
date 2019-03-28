#include "lk.h"

static char * l_process_signal = NULL;
static int32  l_process_signal_type = 0;

// module_init -----------------
static status module_init( void )
{
	log_init();
	config_init();
	l_signal_init();	// no end
	process_init();		// no end
	listen_init();
	event_init();
	ssl_init();			// no end
	serv_init();

	socks5_server_init(); 	// no end
	socks5_local_init(); 	// no end
	return OK;
}
// modules_end ----------------
status modules_end( void )
{
	debug_log("%s --- ", __func__ );
	log_end();
	config_end();
	listen_end();
	event_end();
	serv_end();
	return OK;
}
// dynamic_module_init ------------
status dynamic_module_init( void )
{
	net_init();
	timer_init();
	event_process_init();
	return OK;
}
// dynamic_module_end ------------
status dynamic_module_end( void )
{
	debug_log("%s --- ", __func__ );
	net_end();
	timer_end();
	event_process_end();
	return OK;
}
// lk_daemon ---------------
static status lk_daemon( void )
{
    int32  fd;
	status rc;

	if( !conf.daemon ) {
		return OK;
	}
	rc = fork( );
	switch( rc ) {
		case ( ERROR ):
			err_log("%s --- fork", __func__ );
			break;
		case ( 0 ):
			break;
		default:
			exit(0);
	}

    if (setsid() == ERROR ) {
        err_log("%s --- setsid", __func__ );
        return ERROR;
    }
    umask(0);
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        err_log("%s --- open /dev/null", __func__ );
        return ERROR;
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        err_log( "%s --- dup2(STDIN) failed", __func__ );
        return ERROR;
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        err_log( "%s --- dup2(STDOUT) failed", __func__ );
        return ERROR;
    }
#if 0
    if (dup2(fd, STDERR_FILENO) == -1) {
        err_log( "%s --- dup2(STDERR) failed", __func__ );
        return ERROR;
    }
#endif
    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            err_log( "%s --- close() failed", __func__ );
            return ERROR;
        }
    }
    return OK;
}
// create_pid_file -------------
static status create_pid_file(  )
{
	int32 pid_file;
	char  str[1024] = {0};
	ssize_t rc;

	pid_file = open( L_PATH_PIDFILE, O_CREAT | O_RDWR | O_TRUNC, 0644 );
	if( pid_file == ERROR ) {
		err_log("%s --- pidfile open", __func__ );
		return ERROR;
	}
	snprintf( str, sizeof(str) - sizeof('\0'), "%d", getpid() );
	rc = write( pid_file, str, strlen(str) );
	if( rc == ERROR ) {
		err_log("%s --- write pid to pidfile", __func__ );
		return ERROR;
	}
	close( pid_file );
	return OK;
}
// delete_pid_file --------------
static status delete_pid_file(  )
{
	unlink( L_PATH_PIDFILE );
	return OK;
}
// do_option -------------
static status do_option(  )
{
	if( OK != l_signal_self( l_process_signal_type ) ) {
		err_log("%s --- signal to self failed", __func__ );
		return ERROR;
	}
	return OK;
}
// get_option --------------
static status get_option( int argc, char * argv[] )
{
	if( argc < 2 ) {
		return OK;
	}
	if( argc > 2 ) {
		err_log("too many parameter" );
		return ERROR;
	}
	l_process_signal = argv[1];
	if( *l_process_signal++ != '-' ) {
		err_log("invaild parameter" );
		return ERROR;
	}
	if ( strcmp( l_process_signal, "reload" ) == 0 ) {
		l_process_signal_type = 1;
		return OK;
	} else if( strcmp( l_process_signal, "stop" ) == 0 ) {
		l_process_signal_type = 2;
		return OK;
	}
	err_log("invaild parameter" );
	return ERROR;
}
// main ------------------
int32 main( int argc, char * argv[] )
{
	int32 rc = ERROR;

	conf.http_access_log = 1;
	conf.log_error = 1;
	conf.log_debug = 1;
	l_time_update( );
	if( OK != get_option( argc, argv ) ) {
		err_log("get option", __func__ );
		return ERROR;
	}
	if( l_process_signal ) {
		return do_option( );
	}
	test_init( );
	test_run( );
	if( OK != module_init( ) ) {
		err_log("static module init");
		return ERROR;
	}
	lk_daemon( );
	if( OK != create_pid_file( ) ) {
		err_log("create pid file" );
		goto over;
	}
	if( OK != listen_start( ) ) {
		err_log( "listen_start failed" );
		listen_stop( );
		goto over;
	}
	if( conf.worker_process ) {
		process_master_run( );
	} else {
		process_single_run( );
	}
over:
	modules_end( );
	delete_pid_file( );
	return rc;
}

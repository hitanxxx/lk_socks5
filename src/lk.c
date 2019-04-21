#include "lk.h"

static char * l_process_signal = NULL;
static int32  l_process_signal_type = 0;

modules_init_t init_modules[] = {
	{log_init,			"log"},
	{config_init,		"config"},
	{l_signal_init,		"signal"},	// no end
	{process_init,		"process"},	// no end
	{listen_init,		"listen"},
<<<<<<< HEAD
=======
	{event_init,		"event"},
>>>>>>> 929c97a249c1bc5f3b958bfbe27aa9619335716d
	{ssl_init,			"ssl"},		// no end
	{serv_init,			"serv"},
	{net_init,			"net"},
	{timer_init,		"timer"},
		
	{socks5_server_init,	"socks5_serv"},	// no end
	{socks5_local_init,		"socks5_local"},// no end
	{NULL,	NULL}
};
// module_init -----------------
static status module_init( void )
{
	int i = 0;
	
	while( init_modules[i].pt != NULL ) {
		if( OK != init_modules[i].pt() ) {
			err("modules init failed [%s]\n", init_modules[i].str );
			return ERROR;
		}
		i++;
	}
	return OK;
}
// modules_end ----------------
status modules_end( void )
{
	debug("running\n");
	log_end();
	config_end();
	listen_end();
	
	serv_end();

	net_end();
	timer_end();
	return OK;
}
// dynamic_module_init ------------
status dynamic_module_init( void )
{
<<<<<<< HEAD
	event_init();
=======
>>>>>>> 929c97a249c1bc5f3b958bfbe27aa9619335716d
	return OK;
}
// dynamic_module_end ------------
status dynamic_module_end( void )
{
<<<<<<< HEAD
	event_end();
=======
>>>>>>> 929c97a249c1bc5f3b958bfbe27aa9619335716d
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
			err("fork\n" );
			break;
		case ( 0 ):
			break;
		default:
			exit(0);
	}

    if (setsid() == ERROR ) {
        err("setsid\n");
        return ERROR;
    }
    umask(0);
    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        err("open /dev/null\n");
        return ERROR;
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        err( "dup2(STDIN) failed\n");
        return ERROR;
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        err( "dup2(STDOUT) failed\n");
        return ERROR;
    }
#if 0
    if (dup2(fd, STDERR_FILENO) == -1) {
        err( "%s --- dup2(STDERR) failed", __func__ );
        return ERROR;
    }
#endif
    if (fd > STDERR_FILENO) {
        if (close(fd) == -1) {
            err( "close() failed\n");
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
		err("pidfile open\n");
		return ERROR;
	}
	snprintf( str, sizeof(str) - sizeof('\0'), "%d", getpid() );
	rc = write( pid_file, str, strlen(str) );
	if( rc == ERROR ) {
		err(" write pid to pidfile\n" );
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
		err("signal to self failed\n");
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
		err("too many parameter\n" );
		return ERROR;
	}
	l_process_signal = argv[1];
	if( *l_process_signal++ != '-' ) {
		err("invaild parameter\n" );
		return ERROR;
	}
	if ( strcmp( l_process_signal, "reload" ) == 0 ) {
		l_process_signal_type = 1;
		return OK;
	} else if( strcmp( l_process_signal, "stop" ) == 0 ) {
		l_process_signal_type = 2;
		return OK;
	}
	err("invaild parameter\n" );
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
		err("get option\n");
		return ERROR;
	}
	if( l_process_signal ) {
		return do_option( );
	}
	test_init( );
	test_run( );
	if( OK != module_init( ) ) {
		err("static module init\n");
		return ERROR;
	}
	lk_daemon( );
	if( OK != create_pid_file( ) ) {
		err("create pid file\n" );
		goto over;
	}
	if( OK != listen_start( ) ) {
		err( "listen_start failed\n" );
<<<<<<< HEAD
=======
		listen_stop( );
>>>>>>> 929c97a249c1bc5f3b958bfbe27aa9619335716d
		goto over;
	}
	if( conf.worker_process > 0 ) {
		process_master_run( );
	} else {
		process_single_run( );
	}
over:
	modules_end( );
	delete_pid_file( );
	return rc;
}

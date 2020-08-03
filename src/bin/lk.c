#include "l_base.h"
#include "l_module.h"
#include "l_dns.h"
#include "l_socks5_server.h"
#include "l_socks5_local.h"
#include "l_test.h"

static char * g_opt_str = NULL;
static int32  g_opt_type = 0;

static status lk_daemon( void )
{
    int32  fd;
	status rc;

	if( !conf.base.daemon ) {
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
        err(" dup2(STDERR) failed\n" );
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

static status delete_pid_file(  )
{
	unlink( L_PATH_PIDFILE );
	return OK;
}

static status get_option( int argc, char * argv[] )
{
	if( argc < 2 ) 
	{
		return OK;
	}
	if( argc > 2 ) 
	{
		err("too many parameter [%d], only support 1 args\n", argc - 1 );
		return ERROR;
	}
	g_opt_str = argv[1];
	if( *g_opt_str++ != '-' ) 
	{
		err("formast error\n" );
		return ERROR;
	}
	if ( strcmp( g_opt_str, "reload" ) == 0 ) 
	{
		g_opt_type = 1;
	} 
	else if( strcmp( g_opt_str, "stop" ) == 0 ) 
	{
		g_opt_type = 2;
	}
	else
	{
		err("invaild parameter\n" );
		return ERROR;
	}
	if( g_opt_type != 0 ) 
	{
		pid_t self_pid;
		if( OK != process_self_pid( &self_pid ) )
		{
			err("get self pid failed, errno [%d]\n", errno );
			return ERROR;
		}

		if( OK != process_send_signal( self_pid, g_opt_type ) )
		{
			err("send signal to self failed\n");
			return ERROR;
		}
		exit (0);
	}
	return OK;
}


int32 main( int argc, char * argv[] )
{
	int32 rc = ERROR;	

	// set init log value
	conf.log.error = 1;
	conf.log.debug = 1;

	// get time info
	l_time_update( );

	// get cmd options
	if( OK != get_option( argc, argv ) ) 
	{
		err("get option\n");
		return ERROR;
	}
	
	if( OK != module_init( ) ) 
	{
		err("static module init\n");
		return ERROR;
	}
	lk_daemon( );
	
	create_pid_file( );
	if( OK != listen_start( ) ) 
	{
		err( "listen_start failed\n" );
		goto over;
	}
	/*
	 unit test run..
	 */
	test_start( );
	
	if( conf.base.worker_process > 0 ) 
	{
		process_master_run( );
	} 
	else 
	{
		process_single_run( );
	}
over:
	modules_end( );
	delete_pid_file( );
	return rc;
}

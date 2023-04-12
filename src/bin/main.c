#include "common.h"
#include "modules.h"
#include "test_main.h"

static char * g_opt_str = NULL;
static int32  g_opt_type = 0;

static status proc_daemon( void )
{
    int32  fd;
	status rc = -1;

	if( !config_get()->sys_daemon ) 
	{
		return OK;
	}
	rc = fork( );
	if( rc < 0 )
	{
		err("fork failed, [%d]\n", errno );
		return ERROR;
	}
	else if ( rc == 0 )
	{
		// child
		if (setsid() == ERROR ) 
		{
	        err("setsid\n");
	        return ERROR;
	    }
		umask(0);
		fd = open("/dev/null", O_RDWR);
	    if (fd == -1) 
		{
	        err("open /dev/null failed, [%d]\n", errno );
	        return ERROR;
	    }
	    if (dup2(fd, STDIN_FILENO) == -1)
		{
	        err( "dup2(STDIN) failed, [%d]\n", errno );
	        return ERROR;
	    }
	    if (dup2(fd, STDOUT_FILENO) == -1) 
		{
	        err( "dup2(STDOUT) failed. [%d]\n", errno );
	        return ERROR;
	    }
#if 0
	    if (dup2(fd, STDERR_FILENO) == -1)
		{
	        err(" dup2(STDERR) failed\n" );
	        return ERROR;
	    }
#endif
		if (fd > STDERR_FILENO) 
		{
        	if (close(fd) == -1) 
			{
            	err( "close() failed. [%d]\n", errno );
            	return ERROR;
        	}
    	}
	}
	else if ( rc > 0 )
	{
		exit(0);
	}
    return OK;
}

static status proc_pid_create(  )
{
	int32 pid_file;
	char  str[1024] = {0};
	ssize_t rc;

	pid_file = open( L_PATH_PIDFILE, O_CREAT | O_RDWR | O_TRUNC, 0644 );
	if( pid_file == -1 ) 
	{
		err("pidfile open failed. [%d]\n", errno );
		return ERROR;
	}
	snprintf( str, sizeof(str) - sizeof('\0'), "%d", getpid() );
	rc = write( pid_file, str, strlen(str) );
	if( rc == -1 ) 
	{
		err("write pid str [%s] to pidfile failed. [%d]\n", str, errno );
		return ERROR;
	}
	close( pid_file );
	return OK;
}

static status proc_pid_free(  )
{
	unlink( L_PATH_PIDFILE );
	return OK;
}

static status proc_cmdline_option( int argc, char * argv[] )
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
		err("parameter format error, should be -reload/-stop\n" );
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
		err("parameter format error, should be (-reload/-stop)\n" );
		return ERROR;
	}
	if( g_opt_type != 0 ) 
	{
		pid_t self_pid;
		if( OK != proc_self_pid( &self_pid ) )
		{
			err("get self pid failed, errno [%d]\n", errno );
			return ERROR;
		}

		if( OK != proc_send_signal( self_pid, g_opt_type ) )
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

	setpriority(PRIO_PROCESS, 0, -20 );

	// get time info
	systime_update( );

	// get cmd options
	if( OK != proc_cmdline_option( argc, argv ) ) 
	{
		err("get option failed\n");
		return -1;
	}

	if( OK !=config_init() )
	{
		err("config init failed\n");
		return -1;
	}

	if( OK != proc_daemon( ) )
	{
		err("daemon failed\n");
		return -1;
	}

	if( OK != proc_pid_create( ) )
	{
		err("pid create failed\n");
		return -1;
	}

	do 
	{
		// modules core 
		if( OK != core_modules_init( ) ) 
		{
			err("static module init\n");
			break;
		}

		if( config_get()->sys_process > 0 ) 
		{
			proc_master_run( );
		} 
		else 
		{
			proc_worker_run( );
		}
		
	} while(0);

	core_modules_end();
	proc_pid_free( );
	config_end();
	return rc;
}

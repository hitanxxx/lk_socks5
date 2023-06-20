#include "common.h"
#include "modules.h"
#include "test_main.h"

static char * g_opt_str = NULL;
static int32  g_opt_type = -1;

static status proc_daemon( void )
{
    int32  fd;
	status rc = -1;

	if( !config_get()->sys_daemon ) return OK;

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
		exit(EXIT_SUCCESS);
	}
    return OK;
}

/// @brief storge pid file into file 
/// @return 
static status proc_pid_create(  )
{
	int32 pid_file;
	char  str[128] = {0};
	ssize_t rc;

	pid_file = open( L_PATH_PIDFILE, O_CREAT|O_RDWR|O_TRUNC, 0644 );
	if( pid_file == -1 ) 
	{
		err("pidfile open failed. [%d]\n", errno );
		return ERROR;
	}
	snprintf( str, sizeof(str) - sizeof('\0'), "%d", getpid() );
	rc = write( pid_file, str, strlen(str) );
	close( pid_file );
	if( rc == -1 ) 
	{
		err("pidfile write failed. [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

/// @brief delete pid file
/// @return 
static void proc_pid_free(  )
{
	unlink( L_PATH_PIDFILE );
}

/// @brief process cmd line params
/// @param argc 
/// @param argv 
/// @return 
static status proc_cmd_option( int argc, char * argv[] )
{
    pid_t self_pid;

    /// no argv, normal startup
    if( argc < 2 ) return OK;
    
    /// argv process
    do 
    {
        if( argc > 2 )
        {
            err("argc [%d] to many, only support 1 parameter\n");
            break;
        }

        g_opt_str = argv[1];
        if( strncmp( g_opt_str, "-reload", strlen("-reload") ) == 0 )
            g_opt_type  = 1;
        else if ( strncmp( g_opt_str, "-stop", strlen("-stop") ) == 0 )
            g_opt_type  = 2;
        else
        {
            err("not support parameter [%s]\n", g_opt_str );
            break;
        }
        
        if( OK != proc_pid_form_file( &self_pid ) )
        {
            err("get current process failed, errno [%d]\n", errno );
            break;
        }
        if( OK != proc_signal_send( self_pid, g_opt_type ) )
        {
            err("send signal to self failed\n");
            break;
        }
    } while(0);
    
	/// if process cmdline, always exit 
    exit(EXIT_SUCCESS);
    return OK;
}


int32 main( int argc, char * argv[] )
{
	int32 rc = ERROR;	

	prctl(PR_SET_NAME, "s5");
	setpriority(PRIO_PROCESS, 0, -20 );

	// get time info
	systime_update( );

	// get cmd options
	if( OK != proc_cmd_option( argc, argv ) ) 
	{
		err("proc_cmd_option failed\n");
		return -1;
	}

	/// reset process name
	size_t space = 0;
	int i = 0;
        for (i = 0; i < argc; i++) 
	{
        size_t length = strlen(argv[i]);
        space += length + 1;
    }
    memset(argv[0], '\0', space); // wipe existing args
    strncpy(argv[0], "s5", space - 1); 

	if( OK != config_init() )
	{
		err("config_init failed\n");
		return -1;
	}

	if( OK != proc_daemon( ) )
	{
		err("proc_daemon failed\n");
		return -1;
	}

	if( OK != proc_pid_create( ) )
	{
		err("proc_pid_create failed\n");
		return -1;
	}

	do 
	{
		// modules core 
		if( OK != modules_core_init( ) ) 
		{
			err("modules_core_init failed\n");
			break;
		}

		(config_get()->sys_process_num > 0) ? proc_master_run( ) : proc_worker_run( );
	} while(0);

	modules_core_exit();
	proc_pid_free();
	return rc;
}

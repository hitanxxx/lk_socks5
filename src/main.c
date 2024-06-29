#include "common.h"
#include "modules.h"
#include "test_main.h"

int proc_daemon( void )
{
    if(!config_get()->sys_daemon) 
        return 0;

    int rc = fork();
    if(rc < 0) {
        err("fork failed, [%d]\n", errno);
        return -1;
    } else if(rc == 0) { ///child
        if(setsid() == ERROR) {
            err("setsid failed. [%d]\n", errno);
            return -1;
        }
        umask(0);
        int fd = open("/dev/null", O_RDWR);
        if(fd == -1) {
            err("open /dev/null failed, [%d]\n", errno);
            return -1;
        }
        if(dup2(fd, STDIN_FILENO) == -1) {
            err("dup2(STDIN) failed, [%d]\n", errno);
            return -1;
        }
        if(dup2(fd, STDOUT_FILENO) == -1) {
            err("dup2(STDOUT) failed. [%d]\n", errno);
            return -1;
        }
#if 0
        if (dup2(fd, STDERR_FILENO) == -1) {
            err("dup2(STDERR) failed\n");
            return -1;
        }
#endif
        close(fd);
    } else if(rc > 0) { ///parent
        exit(EXIT_SUCCESS);
    }
    return 0;
}

/// @brief storge pid file into file 
/// @return 
int proc_pid_create()
{
    char str[32] = {0};

    int fd = open(S5_PATH_PID, O_CREAT|O_RDWR|O_TRUNC, 0644);
    if(fd == -1) {
        err("pidfile [%s] open failed. [%d]\n", S5_PATH_PID, errno);
        return -1;
    }
    sprintf(str, "%d", getpid());
    int writen = write(fd, str, strlen(str));
    close(fd);
    if(writen != strlen(str)) {
        err("pidfile [%s] writen [%d]. all [%d]\n", S5_PATH_PID, writen, strlen(str));
        return -1;
    }
    return 0;
}

/// @brief process cmd line params
/// @param argc 
/// @param argv 
/// @return 
status option_parse(int argc, char * argv[])
{
    pid_t pid;
    char * opt_string = NULL;
    int opt_type = -1;
    
    if(argc < 2) return 0; ///common startup
    if(argc > 2) {
        ahead_dbg("argc [%d] too much. only support 1 parameter\n", argc);
        exit(EXIT_SUCCESS);
    }
    
    do {
        opt_string = argv[1];
        if((strlen("-reload") == strlen(opt_string)) && !strncmp("-reload", opt_string, strlen("-reload"))) {
            opt_type = 1;
        } else if ((strlen("-stop") == strlen(opt_string)) && !strncmp("-stop", opt_string, strlen("-stop"))) {
            opt_type = 2;
        } else {
            ahead_dbg("argv [%s] not support\n", opt_string);
            break;
        }
        if(0 != proc_pid_form_file(&pid)) {
            ahead_dbg("get pid of current running program err, [%d]\n", errno);
            break;
        }
        if(0 != proc_signal_send(pid, opt_type)) {
            ahead_dbg("send signal to current running program err\n");
            break;
        }
    } while(0);
    exit(EXIT_SUCCESS); ///process cmdline, always exit 
    return 0;
}

#if defined (TEST)
int main(int argc, char ** argv)
{
    systime_update();
    test_start();
    exit(EXIT_SUCCESS);
}
#else
int main(int argc, char ** argv)
{
	ahead_dbg("Welcome to <sssss>, buildts <%s %s>\n", __DATE__, __TIME__);
    ahead_dbg(" /\\_/\\\n");   
    ahead_dbg("( o.o )\n"); 
    ahead_dbg(" > ^ <\n");

    size_t space = 0;
    int i = 0;
    for(i = 0; i < argc; i++) {
        size_t length = strlen(argv[i]);
        space += length + 1;
    }
    memset(argv[0], '\0', space);    ///wipe existing args
    strncpy(argv[0], "<sssss>", space-1); 
    
    setpriority(PRIO_PROCESS, 0, -20); ///set higiest priority
    systime_update();

    if(0 != option_parse(argc, argv)) {
        ahead_dbg("option_parse failed\n");
        return -1;
    }
    if(0 != config_init()) {
        err("config_init failed\n");
        return -1;
    }
    if(0 != proc_daemon()) {
        err("proc_daemon failed\n");
        return -1;
    }
    if(0 != proc_pid_create()) {
        err("proc_pid_create failed\n");
        return -1;
    }
    if(0 != modules_core_init()) {
        err("modules_core_init failed\n");
        unlink(S5_PATH_PID);
        return -1;
    }
    config_get()->sys_process_num > 0 ? proc_master_run() : proc_worker_run();
    modules_core_exit();
    unlink(S5_PATH_PID);
    return 0;
}
#endif

#include "common.h"
#include "modules.h"

#define L_PROCESS_MASTER 0xff

typedef struct process_ctx
{
    sys_shm_t shm;
    int process_id;
    process_t*  processes;

    int signal;
    sig_atomic_t        sig_quit;
    sig_atomic_t        sig_reap;
    sig_atomic_t        sig_reload;
} process_ctx_t;

static process_ctx_t * g_proc_ctx = NULL;


status proc_pid( )
{
    return g_proc_ctx->processes[g_proc_ctx->process_id].pid;
}

int proc_pid_form_file( pid_t * pid )
{
    char str[32] = {0};
    int fd = open(S5_PATH_PID, O_RDONLY);
    schk(fd > 0, return -1);
    schk(read(fd, str, sizeof(str)) > 0, {close(fd); return -1;});
    *pid = strtol(str, NULL, 10);
    return 0;
}

int proc_signal_send(pid_t pid, int32 signal)
{
    schk(0 == kill(pid, signal), return -1);
    return 0;
}

static int proc_signal_bcast(int sig)
{
    int i = 0;
    for(i = 0; i < config_get()->sys_process_num; i++) {
        if(-1 == proc_signal_send(g_proc_ctx->processes[i].pid, sig)) {
            err("broadcast signal failed\n");
            return -1;
        }
    }
    return 0;
}

void proc_worker_run( void )
{
    int32 timer;
    sigset_t set;
    sigemptyset(&set); ///clear signal set
    sigprocmask(SIG_SETMASK, &set, NULL); ///worker process set the empty signal set to block. it is equal to not block any signal

    modules_process_init();  ///init process modules
    for(;;) {
        if(g_proc_ctx->sig_quit) {
            g_proc_ctx->sig_quit = 0;
            break;
        }
        timer_expire(&timer);
        event_run(timer);
    }
    modules_pocess_exit();
}

static int proc_fork(process_t * process)
{
    pid_t pid = fork();
    if(pid < 0) {
        err("fork child failed, [%d]\n", errno);
        return -1;
    } else if(pid == 0) { ///child
        g_proc_ctx->process_id = process->sequence_num;
        proc_worker_run();
    } else if(pid > 0) { ///parent
        process->pid = pid;
    }
    return 0;
}

void proc_master_run(void)
{
    int i;
    sigset_t set;

    ///blocking this kind of signals
    ///signal will be reprocess when blocking is remove
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    if(sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        err("master blcok signal set failed, [%d]\n", errno);
        return;
    }

    for(i = 0; i < config_get()->sys_process_num; i++) { /// start worker process
        if(-1 == proc_fork(&g_proc_ctx->processes[i])) {
            err("process spawn number [%d] child failed, errno [%d]\n", i, errno);
            return;
        }
        if(g_proc_ctx->process_id != L_PROCESS_MASTER)
            return;
    }
    
    sigemptyset(&set); ///clear signal set
    for(;;) {
        ///sigsuspend is a automic option
        ///1. clear all block signal
        ///2. wait a signal recvied 
        ///and when it return. will recovery signal block to before it called
        sigsuspend(&set);
        systime_update();
        err("master received signal [%d]\n", g_proc_ctx->signal);
        
        if(g_proc_ctx->sig_quit == 1) { ///master recvied a sigint, stop all child frist, then do exit            
            proc_signal_bcast(SIGINT);
            int alive = 0;
            for(i = 0; i < config_get()->sys_process_num; i++) {
                if(!g_proc_ctx->processes[i].exited) {
                    alive++;
                }
            }
            if(alive == 0)
                break;
        }

        if(g_proc_ctx->sig_reap == 1) { ///someone child dead
            for(i = 0; i < config_get()->sys_process_num; i++) {
                if(g_proc_ctx->processes[i].exited) {
                    if(!g_proc_ctx->sig_quit) {  ///if master not recived sigint, just to restart the child process
                        if(-1 == proc_fork( &g_proc_ctx->processes[i])) {
                            err("proc_fork index [%d] failed, [%d]\n", i, errno);
                            continue;
                        }

                        if(g_proc_ctx->process_id != L_PROCESS_MASTER) {
                            return;
                        } else {
                            g_proc_ctx->processes[i].exited = 0;
                        }
                    }
                }
            }
            g_proc_ctx->sig_reap = 0;
        }
        
        if(g_proc_ctx->sig_reload) {
            /// master recived sigreload, just stop all child process.
            /// child process will be auto restart
            proc_signal_bcast(SIGINT);
            g_proc_ctx->sig_reload = 0;
        }
    }
}

void proc_waitpid( )
{
    int i;
    for(;;) {
        ///wait to get anyone child dead pid and no block
        pid_t dead_child_pid = waitpid(-1, NULL, WNOHANG);
        if(dead_child_pid == 0) { /// no any child dead. (some error happen)
            return;
        } else if(dead_child_pid == -1) {
            if(errno == EINTR) ///irq by signal
                continue;
            return;
        }
        for(i = 0; i < config_get()->sys_process_num; i++) {
            if(dead_child_pid == g_proc_ctx->processes[i].pid) {
                g_proc_ctx->processes[i].exited = 1;
                break;
            }
        }
    }
    return;
}

void proc_signal_cb(int signal)
{
    int err_cc = errno; ///cache errno 
    
    g_proc_ctx->signal = signal;
    if(g_proc_ctx->process_id == L_PROCESS_MASTER) { ///master
        if(signal == SIGINT) {
            g_proc_ctx->sig_quit = 1;
        } else if(signal == SIGCHLD) {
            g_proc_ctx->sig_reap = 1;
            proc_waitpid();
        } else if (signal == SIGHUP) {
            g_proc_ctx->sig_reload = 1;
        }
    } else { ///worker
        if(signal == SIGINT)
            g_proc_ctx->sig_quit = 1;
    }
    errno = err_cc; ///recovery errno
}

status proc_signal_init()
{
    int i;
    struct sigaction sa;
    int sig_arr[] = {
        SIGINT,
        SIGHUP,
        SIGCHLD,
        SIGPIPE,
        SIGUSR1,
        SIGUSR2,
        0
    };
    for(i = 0; sig_arr[i]; i++)  {
        memset(&sa, 0, sizeof(struct sigaction));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = proc_signal_cb;
        sa.sa_flags = SA_SIGINFO;
        schk(0 == sigaction(sig_arr[i], &sa, NULL), return -1);
    }
    return 0;
}
int process_init(void)
{
    int i = 0;
    schk(!g_proc_ctx, return -1);
    schk(0 == proc_signal_init(), return -1);
    schk(g_proc_ctx = sys_alloc(sizeof(process_ctx_t)), return -1);

    g_proc_ctx->process_id = L_PROCESS_MASTER;
    g_proc_ctx->shm.size += (config_get()->sys_process_num * sizeof(process_t));
    g_proc_ctx->shm.size += sizeof(int);
    schk(0 == sys_shm_alloc(&g_proc_ctx->shm, g_proc_ctx->shm.size), return -1);
    g_proc_ctx->processes = (process_t*)g_proc_ctx->shm.data;
    for(i = 0; i < config_get()->sys_process_num; i++)
        g_proc_ctx->processes[i].sequence_num = i;
    return 0;
}

status process_end( void )
{
    if(g_proc_ctx) {
        if(g_proc_ctx->shm.size > 0 && g_proc_ctx->shm.data) {
            sys_shm_free(&g_proc_ctx->shm);
            g_proc_ctx->shm.size = 0;
        }
        sys_free(g_proc_ctx);
    }
    return 0;
}

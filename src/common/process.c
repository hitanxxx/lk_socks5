#include "common.h"
#include "modules.h"

process_ctx_t *g_proc_ctx = NULL;

static int proc_signal_bcast(int sig) {
    for (int i = 0; i < config_get()->sys_process_num; i++) {
        if (!g_proc_ctx->parr[i].exited) {
            schk(0 == kill(g_proc_ctx->parr[i].pid, sig), return -1);
        }
    }
    return 0;
}

void proc_worker_task(void) {
    sigset_t set;
    sigemptyset(&set); /// clear signal set
    sigprocmask(SIG_SETMASK, &set, NULL);
    /// worker process set the empty signal set to block. it
    /// is equal to not block any signal

    modules_process_init(); /// init process modules
    for (;;) {
        if (g_proc_ctx->sig_quit) {
            g_proc_ctx->sig_quit = 0;
            break;
        }
        uint64_t ms = 0;
        timer_remaining(&ms);
        ev_loop(ms);
    }
    modules_pocess_exit();
}

static int proc_fork(process_t *process) {
    pid_t pid = fork();
    if (pid < 0) {
        err("fork child failed, [%d]\n", errno);
        return -1;
    } else if (pid == 0) { /// child
        g_proc_ctx->pmaster = 0;
        g_proc_ctx->pcur = process;
        g_proc_ctx->pcur->pid = getpid();
        proc_worker_task();
    } else if (pid > 0) { /// parent
        process->pid = pid;
    }
    return 0;
}

void proc_master_task(void) {
    int i = 0;
    sigset_t set;

    ///block signal 
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        err("master blcok signal set failed, [%d]\n", errno);
        return;
    }

    for (i = 0; i < config_get()->sys_process_num; i++) {
        if (-1 == proc_fork(&g_proc_ctx->parr[i])) {
            err("process spawn number [%d] child failed, errno [%d]\n", i, errno);
            return;
        }
        if (!g_proc_ctx->pmaster) return;
    }

    sigemptyset(&set);
    for (;;) {
        sigsuspend(&set);  ///Unblock all signal && wait signal

        systime_update();
        dbg("master received signal [%d]\n", g_proc_ctx->signal);

        ///master quit -> stop all child process
        if (g_proc_ctx->sig_quit == 1) {
            proc_signal_bcast(SIGINT);
            
            int alive = 0;
            for (i = 0; i < config_get()->sys_process_num; i++) {
                if (!g_proc_ctx->parr[i].exited) {
                    alive++;
                }
            }
            if (alive == 0)
                break;
        }

        ///some child process dead
        if (g_proc_ctx->sig_reap == 1) {
            for (i = 0; i < config_get()->sys_process_num; i++) {
                if (g_proc_ctx->parr[i].exited) {
                    if (-1 == proc_fork(&g_proc_ctx->parr[i])) {
                        err("proc_fork index [%d] failed, [%d]\n", i, errno);
                        continue;
                    }

                    if (!g_proc_ctx->pmaster) {
                        return;
                    } else {
                        g_proc_ctx->parr[i].exited = 0;
                    }
                }
            }
            g_proc_ctx->sig_reap = 0;
        }

        if (g_proc_ctx->sig_reload) {
            /*
                This simply kills all child processes;
                the child processes will automatically restart.
            */
            proc_signal_bcast(SIGINT);
            g_proc_ctx->sig_reload = 0;
        }
    }
}

void proc_waitpid(void) {
    int i;
    for (;;) {
        /// wait to get anyone child dead pid and no block
        pid_t dead_child_pid = waitpid(-1, NULL, WNOHANG);
        if (dead_child_pid == 0) { /// no any child dead. (some error happen)
            return;
        } else if (dead_child_pid == -1) {
            if (errno == EINTR) /// irq by signal
                continue;
            return;
        }
        for (i = 0; i < config_get()->sys_process_num; i++) {
            if (dead_child_pid == g_proc_ctx->parr[i].pid) {
                g_proc_ctx->parr[i].exited = 1;
                break;
            }
        }
    }
    return;
}

void proc_signal_cb(int signal) {
    int err_cc = errno; ///cache errno

    g_proc_ctx->signal = signal;
    if (g_proc_ctx->pmaster) { 
        /// master
        if (signal == SIGINT) {
            g_proc_ctx->sig_quit = 1;
        } else if (signal == SIGCHLD) {
            if (!g_proc_ctx->sig_quit) {
                g_proc_ctx->sig_reap = 1;
            }
            proc_waitpid();
        } else if (signal == SIGHUP) {
            g_proc_ctx->sig_reload = 1;
        }
    } else { 
        /// worker
        if (signal == SIGINT)
            g_proc_ctx->sig_quit = 1;
    }
    errno = err_cc; /// recovery errno
}

int proc_signal_init(void) {
    int i;
    struct sigaction sa;
    int sig_arr[] = {SIGINT, SIGHUP, SIGCHLD, SIGPIPE, SIGUSR1, SIGUSR2, 0};
    for (i = 0; sig_arr[i]; i++) {
        memset(&sa, 0, sizeof(struct sigaction));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = proc_signal_cb;
        sa.sa_flags = SA_SIGINFO;
        schk(0 == sigaction(sig_arr[i], &sa, NULL), return -1);
    }
    return 0;
}

int process_init(void) {
    if (!g_proc_ctx) {
        g_proc_ctx = sys_alloc(sizeof(process_ctx_t) + (sizeof(process_t) * config_get()->sys_process_num));
        schk(g_proc_ctx, return -1);
        g_proc_ctx->pmaster = 1;
        for (int i = 0; i < config_get()->sys_process_num; i++) {
            g_proc_ctx->parr[i].seq = i;
        }
        
        proc_signal_init();
    }
    return 0;
}

int process_end(void) {
    if (g_proc_ctx) {
        sys_free(g_proc_ctx);
        g_proc_ctx = NULL;
    }
    return 0;
}

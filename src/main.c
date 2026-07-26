#include "common.h"
#include "modules.h"
#include "test_main.h"

void s5_daemon(void) {
    if (config_get()->sys_daemon) {
        int ret = fork();
        if (ret < 0) {
            err("daemon. fork err. [%d]\n", errno);
            exit(EXIT_FAILURE);
        } else if (ret > 0) {
            exit(EXIT_SUCCESS);
        } else if (ret == 0) {
            if (setsid() < 0) {
                err("daemon. setsid err. [%d]\n", errno);
                exit(EXIT_FAILURE);
            }
            int fd = open("/dev/null", O_RDWR);
            if (fd >= 0) {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
    }
    return;
}

void s5_save_pid(void) {
    char str[32] = {0};
    sprintf(str, "%d", getpid());

    if (0 != sys_file_write_data(S5_PATH_PID, str, strlen(str))) {
        err("save pid. err [%d]\n", errno);
    }
    return;
}

void s5_command(int argc, char **argv) {
    if (argc < 2) return;
    if (argc > 2) {
        ahead_dbg("cmd number [%d] too much. only support 1 parameter\n", argc);
        exit(EXIT_FAILURE);
    }
    ahead_dbg("[%s]\n", argv[1]);
    
    int sig = 0xff;
    char *cmd = argv[1];
    if ((strlen("-reload") == strlen(cmd)) && !strncmp("-reload", cmd, strlen("-reload"))) {
        sig = SIGHUP;
    } else if ((strlen("-stop") == strlen(cmd)) && !strncmp("-stop", cmd, strlen("-stop"))) {
        sig = SIGINT;
    } else {
        ahead_dbg("cmd [%s] not support\n", cmd);
        exit(EXIT_FAILURE);
    }

    sys_data_t *pdata = sys_file_read_value(S5_PATH_PID);
    if (pdata) {
        pid_t pid = strtol(pdata->data, NULL, 10);
        sys_free(pdata);
        if (0 != kill(pid, sig)) {
            ahead_dbg("cmd send sig [%d] to pid [%d] err. [%d]\n", sig, pid, errno);
            exit(EXIT_FAILURE);
        }
    } else {
        ahead_dbg("cmd get pid err. [%d]\n", errno);
        exit(EXIT_FAILURE);
    }
    
    exit(EXIT_SUCCESS); /// process cmdline, always exit
    return;
}

static void s5_rename(int argc, char **argv) {
    size_t space = 0;
    for (int i = 0; i < argc; i++) {
        space += strlen(argv[i]) + 1;
    }
    memset(argv[0], 0x0, space); /// wipe existing args
    strncpy(argv[0], "s5", space - 1);
    return;
}

#if defined(TEST)
int main(int argc, char **argv) {
    systime_update();
    test_start();
    exit(EXIT_SUCCESS);
}
#else
int main(int argc, char **argv) {
    ahead_dbg("Welcome to <S5>, buildts <%s %s>\n", __DATE__, __TIME__);
    ahead_dbg(" /\\_/\\\n");
    ahead_dbg("( o.o )\n");
    ahead_dbg(" > ^ <\n");
    ahead_dbg("OpenSSL ver [%s]\n", OPENSSL_VERSION_TEXT);

    s5_command(argc, argv);
    s5_rename(argc, argv);
    systime_update();
    
    schk(0 == config_init(), return -1);
    schk(0 == log_init(), return -1);
    schk(0 == process_init(), return -1);
    schk(0 == listen_init(), return -1);

    s5_daemon();
    s5_save_pid();
    
    config_get()->sys_process_num > 0 ? proc_master_task() : proc_worker_task();

    schk(0 == log_end(), return -1);
    schk(0 == process_end(), return -1);
    schk(0 == listen_end(), return -1);
    unlink(S5_PATH_PID);
    return 0;
}
#endif

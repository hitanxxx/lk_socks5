#ifndef _PROCESS_H_INCLUDED_
#define _PROCESS_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#define MAXPROCESS 32

typedef struct {
    uint32_t seq;
    pid_t pid;
    uint8_t exited;
} process_t;

typedef struct {
    
    uint8_t     pmaster;
    uint32_t    signal;
    sig_atomic_t sig_quit;
    sig_atomic_t sig_reap;
    sig_atomic_t sig_reload;

    process_t   *pcur;
    process_t   parr[];
} process_ctx_t;


extern process_ctx_t *g_proc_ctx;

void proc_master_task(void);
void proc_worker_task(void);

int process_end(void);
int process_init(void);

#ifdef __cplusplus
}
#endif

#endif

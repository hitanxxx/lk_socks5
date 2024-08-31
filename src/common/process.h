#ifndef _PROCESS_H_INCLUDED_
#define _PROCESS_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define	MAXPROCESS 128

typedef struct {
	uint32			sequence_num;
	pid_t			pid;

	int32			exited;
} process_t ;

void proc_master_run(void);
void proc_worker_run(void);

int proc_pid();
int proc_pid_form_file(pid_t * pid);
int proc_signal_send(pid_t pid, int signal);
int process_end(void);
int process_init(void);

#ifdef __cplusplus
}
#endif
        
#endif

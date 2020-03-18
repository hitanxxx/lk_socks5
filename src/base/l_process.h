#ifndef _L_PROCESS_H_INCLUDED_
#define _L_PROCESS_H_INCLUDED_

#define	MAXPROCESS 128

typedef struct process_t {
	uint32			sequence_num;
	pid_t			pid;

	int32			exited;
} process_t ;

process_t * process_get_run( void );
void process_master_run( void );
void process_single_run( void );

status process_self_pid( pid_t * pid );
status process_send_signal( pid_t pid, int32 signal );



status process_end( void );
status process_init( void );
#endif

#ifndef _PROCESS_H_INCLUDED_
#define _PROCESS_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define	MAXPROCESS 128

typedef struct process_t {
	uint32			sequence_num;
	pid_t			pid;

	int32			exited;
} process_t ;

void proc_master_run( void );
void proc_worker_run( void );

status proc_pid( );
status proc_pid_form_file( pid_t * pid );

status proc_signal_send( pid_t pid, int32 signal );

status process_end( void );
status process_init( void );

void process_lock( );
void process_unlock( );

void process_mutex_value_set( int v );
int process_mutex_value_get( );


#ifdef __cplusplus
}
#endif
        
#endif

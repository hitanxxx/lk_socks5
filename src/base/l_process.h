#ifndef _L_PROCESS_H_INCLUDED_
#define _L_PROCESS_H_INCLUDED_

#define	MAXPROCESS 128

extern uint32		process_num;
extern uint32		process_id;

typedef struct process_t {
	uint32			nid;
	pid_t			pid;

	int32			exiting;
	int32			exited;
} process_t ;
extern process_t *	process_arr;

process_t * process_get_run( void );
void process_master_run( void );
void process_single_run( void );

status process_end( void );
status process_init( void );
#endif

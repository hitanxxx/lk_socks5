#ifndef _L_SIGNAL_H_INCLUDED_
#define _L_SIGNAL_H_INCLUDED_

extern sig_atomic_t		sig_quit;
extern sig_atomic_t		sig_reap;
extern sig_atomic_t 	sig_perf;
extern sig_atomic_t     sig_reload;
extern sig_atomic_t		sig_perf_stop;
extern int32    global_signal;

status l_signal_init( void );
status l_signal_end( void );
status l_signal_self( int32 lsignal );

#endif

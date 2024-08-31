#ifndef _SYS_TIME_H_INCLUDED_
#define _SYS_TIME_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

status systime_update( void );
long long systime_msec( );
char * systime_gmt( );
char * systime_log( );    

#ifdef __cplusplus
}
#endif
        
#endif

#ifndef _L_TIME_H_INCLUDED_
#define _L_TIME_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

extern string_t	cache_time_gmt;
extern string_t	cache_time_log;
extern int64_t	cache_time_msec;

status l_time_update( void );

#ifdef __cplusplus
}
#endif
        
#endif

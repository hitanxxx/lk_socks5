#ifndef _SYS_TIME_H_INCLUDED_
#define _SYS_TIME_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

int systime_update(void);
long long systime_msec(void);
char *systime_gmt(void);
char *systime_log(void);

#ifdef __cplusplus
}
#endif

#endif

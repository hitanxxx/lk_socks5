#ifndef _SYS_TIME_H_INCLUDED_
#define _SYS_TIME_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

void systime_update(void);
uint64_t systime_msec(void);
char *systime_gmt(void);
char *systime_log(void);

#ifdef __cplusplus
}
#endif

#endif

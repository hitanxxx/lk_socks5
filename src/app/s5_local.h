#ifndef _S5_LOCAL_H_INCLUDED_
#define _S5_LOCAL_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

int socks5_local_init(void);
int socks5_local_end(void);
int s5_local_accept_cb(event_t * ev);



#ifdef __cpluscplus
}
#endif

#endif

#ifndef _S5_LOCAL_H_INCLUDED_
#define _S5_LOCAL_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

status socks5_local_init( void );
status socks5_local_end( void );
status socks5_local_accept_cb( event_t * ev );



#ifdef __cpluscplus
}
#endif

#endif

#ifndef _TLS_TUNNEL_C_H_INCLUDED_
#define _TLS_TUNNEL_C_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

int tls_tunnel_c_init(void);
int tls_tunnel_c_exit(void);
int tls_tunnel_c_accept(event_t * ev);


#ifdef __cpluscplus
}
#endif

#endif

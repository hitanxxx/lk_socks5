#ifndef _SEND_H_INCLUDED_
#define _SEND_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
ssize_t sends( con_t * c, unsigned char * buffer, uint32 len );
ssize_t recvs( con_t * c, unsigned char * buffer, uint32 len );
status send_chains( con_t * c , meta_t * send_meta );

ssize_t udp_sends( con_t * c, unsigned char * buffer, uint32 len );
ssize_t udp_recvs( con_t * c, unsigned char * buffer, uint32 len );

#ifdef __cplusplus
}
#endif
    
#endif

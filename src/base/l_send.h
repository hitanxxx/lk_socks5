#ifndef _L_SEND_H_INCLUDED_
#define _L_SEND_H_INCLUDED_

ssize_t sends( connection_t * c, unsigned char * start, uint32 len );
ssize_t recvs( connection_t * c, unsigned char * start, uint32 len );
status send_chains( connection_t * c , meta_t * send_meta );

ssize_t udp_sends( connection_t * c, unsigned char * start, uint32 len );
ssize_t udp_recvs( connection_t * c, unsigned char * start, uint32 len );

#endif

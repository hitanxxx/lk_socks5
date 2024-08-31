#ifndef _SEND_H_INCLUDED_
#define _SEND_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
int sends(con_t * c, unsigned char * buf, int bufn);
int recvs(con_t * c, unsigned char * buf, int bufn);
int send_chains(con_t * c , meta_t * send_meta);

int udp_sends(con_t * c, unsigned char * buf, int bufn);
int udp_recvs(con_t * c, unsigned char * buf, int bufn);

#ifdef __cplusplus
}
#endif
    
#endif

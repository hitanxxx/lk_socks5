#ifndef _L_SEND_H_INCLUDED_
#define _L_SEND_H_INCLUDED_

#define MAX_IVO_NUM 65535


typedef struct send_iovec_t {
	struct	iovec	iovec[MAX_IVO_NUM];
	uint32 	all_len;
	uint32 	iov_count;
} send_iovec_t;

ssize_t sends( connection_t * c, char * start, uint32 len );
ssize_t recvs( connection_t * c, char * start, uint32 len );
status send_chains( connection_t * c , meta_t * send_meta );

ssize_t udp_sends( connection_t * c, char * start, uint32 len );
ssize_t udp_recvs( connection_t * c, char * start, uint32 len );




#endif

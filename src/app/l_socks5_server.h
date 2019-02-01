#ifndef _L_SOCKS5_H_INCLUDED_
#define _L_SOCKS5_H_INCLUDED_

#define SOCKS5_META_LENGTH	4096
#define SOCKS5_TIME_OUT	5

#define SOCKS5_SERVER	0x0001
#define SOCKS5_CLIENT	0x0002

typedef struct socks5_init_t {
	int32 	state;
	char 	ver;
	char 	nmethod;
	int32	offset;
	char 	method[256];
} socks5_init_t;

typedef struct socks5_request_t {
	int32		state;
	char		ver;
	char		cmd;
	char		rsv;
	char		atyp;
	
	char		dst_addr[256];
	int32 		offset;
	char		host_len;
	char		dst_port[2];
} socks5_request_t;

typedef struct socks5_cycle_t {
	socks5_init_t		init;
	socks5_request_t	request;
	connection_t * 		down;
	connection_t * 		up;
	
	net_transport_t * in;
	net_transport_t * out;
} socks5_cycle_t;

status socks5_server_init( void );
status socks5_server_end( void );

void socks5_cycle_time_out( void * data );
void socks5_connection_time_out( void * data );
status socks5_cycle_free( socks5_cycle_t * cycle );
status socks5_pipe( event_t * ev );

#endif
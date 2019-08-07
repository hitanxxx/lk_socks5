#ifndef _L_SOCKS5_H_INCLUDED_
#define _L_SOCKS5_H_INCLUDED_

#define SOCKS5_META_LENGTH	4096
#define SOCKS5_TIME_OUT	3

#define SOCKS5_SERVER	0x0001
#define SOCKS5_CLIENT	0x0002

#define SOCKS5_AUTH_REQ				0x1001
#define SOCKS5_AUTH_RESP			0x1002

#define SOCKS5_AUTH_SUCCESS			0x2000
#define SOCKS5_AUTH_MAGIC_FAIL		0x2001
#define SOCKS5_AUTH_TYPE_FAIL		0x2002
#define SOCKS5_AUTH_NO_USER			0x2003
#define SOCKS5_AUTH_PASSWD_FAIL		0x2004

#define SOCKS5_USER_NAME_MAX		16
#define SOCKS5_USER_PASSWD_MAX		16

typedef struct socks5_user_t 
{
	char		name[SOCKS5_USER_NAME_MAX];
	char 		passwd[SOCKS5_USER_PASSWD_MAX];

	queue_t		queue;
} socks5_user_t ;

typedef struct socks5_auth
{
	int 		magic; 		// 947085
	char 		name[32];
	char 		passwd[32];

	int			auth_type;  // 0:req  1:response 	
	int			auth_resp_code;
} socks5_auth_t;

typedef struct socks5_init_t {
	int32 	state;
	char 	ver;
	char 	nmethod;
	char	m_offset;
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
	
	net_transport_t * local2remote;
	net_transport_t * remote2local;

} socks5_cycle_t;

status socks5_server_init( void );
status socks5_server_end( void );

void socks5_timeout_cycle( void * data );
void socks5_timeout_con( void * data );

status socks5_cycle_free( socks5_cycle_t * cycle );
status socks5_pipe( event_t * ev );

#endif

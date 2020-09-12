#ifndef _L_SOCKS5_H_INCLUDED_
#define _L_SOCKS5_H_INCLUDED_

// socks5 private macro
#define SOCKS5_META_LENGTH			(4096*4)
#define SOCKS5_TIME_OUT				3

#define SOCKS5_AUTH_MAGIC_NUM		947085


/* socks5 message type */
enum socks5_private_auth_message_type {
	SOCKS5_AUTH_REQ,
	SOCKS5_AUTH_RESP
};
/* scoks5 message status */
enum socks5_private_auth_message_status {
	SOCKS5_AUTH_SUCCESS,
	SOCKS5_AUTH_MAGIC_FAIL,
	SOCKS5_AUTH_TYPE_FAIL,
	SOCKS5_AUTH_NO_USER,
	SOCKS5_AUTH_PASSWD_FAIL
};

typedef struct socks5_auth {
	int 		magic; 				// magic num filed
	int			message_type;  		// private auth message type. enum socks5_message_type
	int			message_status;		// private auth message status, enum socks5_message_status

	char 		name[USERNAME_LENGTH];			// private auth username
	char 		passwd[PASSWD_LENGTH];			// private auth passwd
} socks5_auth_t;

typedef struct socks5_message_invite {
	int32 		state;
	char 		ver;
	char 		method_num;
	char		method_n;
	char 		method[256];
} socks5_message_invite_t;

typedef struct socsk5_message_invite_response {
	char		ver;
	char		method;
}__attribute__((packed)) socsk5_message_invite_response_t;

typedef struct socks5_message_advcance {
	int32				state;
	unsigned char		ver;
	unsigned char		cmd;
	unsigned char		rsv;
	unsigned char		atyp;

	unsigned char		addr_len;
	unsigned char		addr_recv;

	unsigned char 		addr_str[256];
	unsigned char		addr_port[2];
} socks5_message_advance_t;

typedef struct socks5_message_advance_response {
	unsigned char			ver;
	unsigned char			rep;
	unsigned char			rsv;
	unsigned char			atyp;
	unsigned int	bnd_addr;
	unsigned short	bnd_port;
}__attribute__((packed)) socks5_message_advance_response_t;

typedef struct socks5_cycle_t 
{
	socks5_message_invite_t		invite;
	socks5_message_advance_t	advance;

	connection_t *		down;
	connection_t *		up;
	
	dns_cycle_t * dns_cycle;

#if defined(S5_UNFIXED_CACHE)
	net_transport_t * local2remote;
	net_transport_t * remote2local;
#else
	char down_recv_error;
	char up_recv_error;
#endif
} socks5_cycle_t;



status socks5_server_init( void );
status socks5_server_end( void );

void socks5_timeout_cycle( void * data );
void socks5_timeout_con( void * data );

status socks5_cycle_free( socks5_cycle_t * cycle );
status socks5_pipe( event_t * ev );

#endif

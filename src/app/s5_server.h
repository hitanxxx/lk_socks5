#ifndef _S5_SERVER_H_INCLUDED_
#define _S5_SERVER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define SOCKS5_TIME_OUT				3
#define S5_AUTH_MAGIC_NUM           0x2314bacd

enum socks5_auth_message_type
{
    S5_AUTH_TYPE_AUTH_REQ       = 0x1,
    S5_AUTH_TYPE_AUTH_RESP      = 0x2,
};
enum socks5_auth_message_status
{
    S5_AUTH_STAT_SUCCESS        = 0x1,
    S5_AUTH_STAT_MAGIC_FAIL     = 0x2,
    S5_AUTH_STAT_TYPE_FAIL      = 0x3,
    S5_AUTH_STAT_NO_USER        = 0x4,
    S5_AUTH_STAT_PASSWD_FAIL    = 0x5
};
enum socks5_req_type
{
	S5_REQ_TYPE_IPV4 	= 0x1,
	S5_REQ_TYPE_IPV6 	= 0x4,
	S5_REQ_TYPE_DOMAIN 	= 0x3,
};



#pragma pack(push,1)
/*
	tips!!!
	protocol struct, need pach(push, 1)
*/
typedef struct
{
    unsigned char           name[USERNAME_LENGTH];  
    unsigned char           passwd[PASSWD_LENGTH];	
	char					resverd[32];
} socks5_auth_data_t;


typedef struct 
{
    uint32_t            magic;                  // magic num filed
    unsigned char       message_type;           // private auth message type. enum socks5_message_type
    unsigned char       message_status;         // private auth message status, enum socks5_message_status
    unsigned short      reserved;

	socks5_auth_data_t	data;
} socks5_auth_header_t;


typedef struct 
{
    int32               state;
    unsigned char       ver;
    unsigned char       method_num;
    unsigned char       method_n;
    unsigned char       method[256];
} socks5_message_invite_t;

typedef struct 
{
    unsigned char       ver;
    unsigned char       method;
} socsk5_message_invite_response_t;

typedef struct 
{
    int32               state;
    unsigned char       ver;
    unsigned char       cmd;
    unsigned char       rsv;
    unsigned char       atyp;

    unsigned char       addr_len;
    unsigned char       addr_recv;

    unsigned char       addr_str[DOMAIN_LENGTH];
    unsigned char       addr_port[2];
} socks5_message_advance_t;

typedef struct 
{
    unsigned char       ver;
    unsigned char       rep;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned int        bnd_addr;
    unsigned short      bnd_port;
} socks5_message_advance_response_t;
#pragma pack(pop)


typedef struct 
{
    queue_t                     queue;
    socks5_message_invite_t     invite;
    socks5_message_advance_t    advance;

    connection_t *              down;
    connection_t *              up;
    dns_cycle_t *               dns_cycle;

	char		recv_down_err;
	char		recv_up_err;
} socks5_cycle_t;


status s5_free( socks5_cycle_t * s5 );
status s5_alloc( socks5_cycle_t ** s5 );
void s5_timeout( void * data );


status socks5_traffic_transfer( event_t * ev );
status socks5_server_secret_start( event_t * ev );
status socks5_server_accept_cb( event_t * ev );

status socks5_server_init( void );
status socks5_server_end( void );


#ifdef __cplusplus
}
#endif
    
#endif

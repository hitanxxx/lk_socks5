#ifndef _S5_SERVER_H_INCLUDED_
#define _S5_SERVER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define S5_TIMEOUT	        5
#define S5_AUTH_MAGIC_NUM   0x1000beef

/// used for private authorization
#define S5_MSG_LOGIN_REQ    0xa1
#define S5_MSG_LOGIN_RESP   0xa2

#define S5_ERR_SUCCESS      0x0
#define S5_ERR_MAGFAIL      0x1
#define S5_ERR_TYPEFAIL     0x2
#define S5_ERR_USERNULL     0x3
#define S5_ERR_PASSFAIL     0x4

/// s5 type for rfc reqest
#define S5_RFC_IPV4         0x1
#define S5_RFC_IPV6         0x4
#define S5_RFC_DOMAIN       0x3


/// struct 1-Byte alignment
#pragma pack(push,1)

/// @brief  struct of private authorization data. 32 Byte
typedef struct s5_auth_data_s
{
    unsigned char           name[USERNAME_LENGTH];  /// 16 Byte
    unsigned char           passwd[PASSWD_LENGTH];	/// 16 Byte
} s5_auth_data_t;

/// @brief struct of private authorization. 8 Byte
typedef struct s5_auth_info_s
{
    uint32_t            magic;                  /// 4 Byte
    unsigned char       msg_type;               /// 1 Byte
    unsigned char       msg_errcode;            /// 1 Byte
    unsigned short      reserved;               /// 2 Byte
} s5_auth_info_t;


typedef struct s5_rfc_phase1_req_s
{
    unsigned char       ver;
    unsigned char       methods_n;
    unsigned char       methods_cnt;
    unsigned char       methods[255];
} s5_rfc_phase1_req_t;

typedef struct s5_rfc_phase1_resp_s
{
    unsigned char       ver;
    unsigned char       method;
} s5_rfc_phase1_resp_t;

typedef struct s5_rfc_phase2_req_s
{
    unsigned char       ver;
    unsigned char       cmd;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned char       dst_addr_n;
    unsigned char       dst_addr_cnt;
    unsigned char       dst_addr[DOMAIN_LENGTH];
    unsigned char       dst_port[2];
} s5_rfc_phase2_req_t;

typedef struct s5_rfc_phase2_resp_s
{
    unsigned char       ver;
    unsigned char       rep;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned int        bnd_addr;
    unsigned short      bnd_port;
} s5_rfc_phase2_resp_t;

#pragma pack(pop)


typedef struct 
{
    queue_t  queue;
    int state;
    s5_rfc_phase1_req_t phase1;
    s5_rfc_phase2_req_t phase2;

    connection_t * down;
    connection_t * up;
    dns_cycle_t * dns_cycle;

	char		recv_down_err;
	char		recv_up_err;
} socks5_cycle_t;


status s5_free( socks5_cycle_t * s5 );
status s5_alloc( socks5_cycle_t ** s5 );
void s5_timeout_cb( void * data );


status socks5_traffic_transfer( event_t * ev );
status socks5_server_secret_start( event_t * ev );
status socks5_server_accept_cb( event_t * ev );

status socks5_server_init( void );
status socks5_server_end( void );


#ifdef __cplusplus
}
#endif
    
#endif

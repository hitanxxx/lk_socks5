#ifndef _L_CONFIG_H_INCLUDED_
#define _L_CONFIG_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define CONF_SETTING_LENGTH		32768

typedef struct socks5_client 
{
    uint32              local_port;
    uint32              server_port;
    unsigned char       server_ip[IPV4_LENGTH];

    unsigned char       user[USERNAME_LENGTH];
    unsigned char       passwd[PASSWD_LENGTH];
} socks5_client;

typedef struct socks5_server 
{
    uint32              server_port;
} socks5_server;

typedef struct http_conf 
{
    uint32              ports[L_OPEN_PORT_MAX];
    uint32              port_n;

    uint32              ssl_ports[L_OPEN_PORT_MAX];
    uint32              ssl_portn;

    unsigned char		home[FILEPATH_LENGTH];
    unsigned char		index[FILEPATH_LENGTH];
} http_conf;

typedef struct gobal_conf
{
    meta_t* 	meta;
    struct base
    {
        uint32              daemon;
        uint32              worker_process;
        unsigned char       sslcrt[FILEPATH_LENGTH];
        unsigned char       sslkey[FILEPATH_LENGTH];
        unsigned char       authfile[FILEPATH_LENGTH];
    } base;

    struct log
    {
        uint32              debug;
        uint32              error;
    } log;

    uint32			        socks5_mode;
    union
    {
        socks5_client       client;
        socks5_server       server;
    } socks5;

    http_conf				http;
}gobal_conf_t;

extern gobal_conf_t conf;

status config_init( void );
status config_end( void );

#ifdef __cplusplus
}
#endif

#endif

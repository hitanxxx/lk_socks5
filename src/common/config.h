#ifndef _CONFIG_H_INCLUDED_
#define _CONFIG_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct gobal_conf
{
    /// sys 
    int         sys_daemon;
    int         sys_process_num;
    int         sys_log_level;

    char         ssl_crt_path[FILEPATH_LENGTH+1];
    char         ssl_key_path[FILEPATH_LENGTH+1];

    /// socks5
    int             s5_mode;
    
    unsigned short     s5_serv_port;
    char            s5_serv_auth_path[FILEPATH_LENGTH+1];
    char            s5_serv_gw[32];

    unsigned short        s5_local_port;
    unsigned short        s5_local_serv_port;
    char                 s5_local_serv_ip[IPV4_LENGTH+1];
    char                s5_local_auth[32+1];

    /// http
    int         http_num;
    int         https_num;
    unsigned short            http_arr[L_OPEN_PORT_MAX];
    unsigned short            https_arr[L_OPEN_PORT_MAX];
    char        http_home[FILEPATH_LENGTH+1];
    char        http_index[FILEPATH_LENGTH+1];
    
}config_t;

config_t * config_get(void);
int config_init(void);
int config_end(void);

#ifdef __cplusplus
}
#endif

#endif

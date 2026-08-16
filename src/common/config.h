#ifndef _CONFIG_H_INCLUDED_
#define _CONFIG_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /// sys
    uint8_t sys_daemon;
    uint8_t sys_process_num;
    uint8_t sys_log_level;

    char ssl_crt_path[FILEPATH_LENGTH];
    char ssl_key_path[FILEPATH_LENGTH];

    /// socks5
    enum socks5_type    s5_mode;
    uint16_t            s5_serv_port;
    char                s5_serv_auth_path[FILEPATH_LENGTH];
    char                s5_serv_gw[32];

    uint16_t            s5_local_port;
    uint16_t            s5_local_serv_port;
    char                s5_local_serv_ip[IPV4_LENGTH];
    char                s5_local_auth[32];

    /// http
    uint8_t http_num;
    uint16_t http_arr[L_OPEN_PORT_MAX];

    uint8_t https_num;
    uint16_t https_arr[L_OPEN_PORT_MAX];

    char http_home[FILEPATH_LENGTH];
    char http_index[FILEPATH_LENGTH];

} config_t;

config_t *config_get(void);
int config_init(void);

#ifdef __cplusplus
}
#endif

#endif

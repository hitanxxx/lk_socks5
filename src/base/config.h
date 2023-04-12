#ifndef _CONFIG_H_INCLUDED_
#define _CONFIG_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define CONF_SETTING_LENGTH		32768


typedef struct gobal_conf
{
    int 		sys_daemon;
    int 		sys_process;
    int 		sys_log_level;

    char 		ssl_crt_path[FILEPATH_LENGTH];
    char 		ssl_key_path[FILEPATH_LENGTH];

    int 		s5_mode;
    int 		s5_serv_port;
    char        s5_serv_auth_path[FILEPATH_LENGTH];

    int			s5_local_port;
    int			s5_local_serv_port;
    char 		s5_local_serv_ip[IPV4_LENGTH];
    char		s5_local_usrname[USERNAME_LENGTH];
    char		s5_local_passwd[PASSWD_LENGTH];

    int         http_num;
    int         https_num;
    int			http_arr[L_OPEN_PORT_MAX];
    int			https_arr[L_OPEN_PORT_MAX];
    char		http_home[FILEPATH_LENGTH];
    char		http_index[FILEPATH_LENGTH];
	
}config_t;

config_t * config_get( void );
status config_init( void );
status config_end( void );

#ifdef __cplusplus
}
#endif

#endif

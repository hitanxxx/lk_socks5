#ifndef _L_CONFIG_H_INCLUDED_
#define _L_CONFIG_H_INCLUDED_

#define 		CONF_SETTING_LENGTH				32768

typedef struct config_t
{
	meta_t* 	meta;
	// global
	uint32		daemon;
	uint32		worker_process;
	string_t	sslcrt;
	string_t	sslkey;
	// log
	uint32		log_debug;
	uint32		log_error;
	// http
	uint32		http_keepalive;
	uint32		http_access_log;
	uint32 		http_n;
	uint32		http[10];
	uint32		https_n;
	uint32		https[10];
	string_t	home;
	string_t	index;
	// tunnel
	uint32		tunnel_mode;
	string_t	serverip;
	// perf
	uint32  	perf_switch;
	// lktp
	uint32		lktp_mode;
	string_t 	lktp_serverip;
	// socks5 
	uint32		socks5_mode;
	string_t 	socks5_serverip;
	uint32		socks5_server_port;
	uint32		socks5_local_port;

}config_t;

extern config_t conf;

status config_get ( meta_t ** meta, char * path );
status config_init( void );
status config_end( void );

#endif

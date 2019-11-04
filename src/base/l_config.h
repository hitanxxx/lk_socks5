#ifndef _L_CONFIG_H_INCLUDED_
#define _L_CONFIG_H_INCLUDED_

#define CONF_SETTING_LENGTH		32768

typedef struct gobal_conf
{
	meta_t* 	meta;
	struct base_ 
{
		uint32		daemon;
		uint32		worker_process;
		char		sslcrt[FILEPATH_LENGTH];
		char		sslkey[FILEPATH_LENGTH];
	} base;

	struct log_ {
		uint32		debug;
		uint32		error;
	} log;

	uint32			socks5_mode;
	struct socks5_client_ {
		uint32		local_port;
		uint32		server_port;
		char		server_ip[IPV4_LENGTH];

		char		user[USERNAME_LENGTH];
		char		passwd[PASSWD_LENGTH];
	} socks5_client;

	struct socks5_server_ {
		uint32		server_port;
		char		authfile[FILEPATH_LENGTH];
	} socks5_server;
}gobal_conf_t;

extern gobal_conf_t conf;

status config_get ( meta_t ** meta, char * path );
status config_init( void );
status config_end( void );

#endif

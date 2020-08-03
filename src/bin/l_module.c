#include "l_base.h"
#include "l_module.h"
#include "l_dns.h"
#include "l_socks5_server.h"
#include "l_socks5_local.h"
#include "l_http_entitybody.h"
#include "l_http_request_head.h"
#include "l_http_response_head.h"
#include "l_webserver.h"

modules_init_t init_modules[] = {
	{log_init,                log_end,                "log"},
	{config_init,             config_end,             "config"},
	{user_init,               user_end,               "usmgr"},
	{process_init,            process_end,            "process"},	
	{listen_init,             listen_end,             "listen"},
	{ssl_init,                ssl_end,                "ssl"},		
	{serv_init,               serv_end,               "serv"},
	{net_init,                net_end,                "net"},
	{timer_init,              timer_end,              "timer"},
		
	{socks5_server_init,      socks5_server_end,      "socks5_serv"},	
	{socks5_local_init,       socks5_local_end,       "socks5_local"},

	{http_response_head_init_module,            http_response_head_end_module,      	  "http_response"},
	{http_request_head_init_module,             http_request_head_end_module,      	 	  "http_request_head"},
	{http_entitybody_init_module,               http_entitybody_end_module,      	 	  "http_entity_body"},
	{webser_init,             webser_end,      	 	  "http"},
	{NULL,	NULL,  NULL}
};

status module_init( void )
{
	int i = 0;
	
	while( init_modules[i].init_pt ) 
	{
		if( OK != init_modules[i].init_pt() ) 
		{
			err("modules [%s] init failed\n", init_modules[i].module_name );
			return ERROR;
		}
		i++;
	}
	return OK;
}

status modules_end( void )
{
	int i = 0;
	
	while( init_modules[i].end_pt ) 
	{
		if( OK != init_modules[i].end_pt() ) 
		{
			err("modules [%s] end failed\n", init_modules[i].module_name );
			return ERROR;
		}
		i++;
	}
	return OK;
}




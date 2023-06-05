#include "common.h"
#include "modules.h"
#include "dns.h"
#include "s5_server.h"
#include "s5_local.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"
#include "mailsender.h"

typedef status ( * module_init_pt )(void);
typedef status ( * module_end_pt )(void);
typedef struct
{
	module_init_pt	init_pt;
	module_end_pt	end_pt;
	char *			module_name;
}modules_init_t;

modules_init_t core_modules[] =
{
	{log_init,                          log_end,                        "log"},
	{process_init,                      process_end,                    "process"},
	{listen_init,                       listen_end,                     "listen"},
	{NULL,	NULL,  NULL}
};


modules_init_t app_modules[] =
{
	{ssl_init,                          ssl_end,                        "ssl"},
    {timer_init,                        timer_end,                      "timer"},
    {net_init,                          net_end,                        "net"},
    {event_init,                        event_end,                      "event"},
	 
    {socks5_server_init,                socks5_server_end,              "socks5_serv"},
    {socks5_local_init,                 socks5_local_end,               "socks5_local"},

    {http_req_init_module,     			http_req_end_module,   			"http_req_head"},
    {http_body_init_module,             http_body_end_module,           "http_body"},
    {mailsender_init,                   mailsender_exit,                "mailsender"},
    {webser_init,                       webser_end,                     "http"},
    {dns_init,                        dns_end,                      "dns"},
    {NULL,	NULL,  NULL}
};

/// @brief core modules means need by all process include master process and worke process
/// @param  
/// @return 
status modules_core_init( void )
{
    int i = 0;
    while( core_modules[i].init_pt )
    {
        if( OK != core_modules[i].init_pt() )
        {
            err("core modules [%s] init failed\n", core_modules[i].module_name );
            return ERROR;
        }
        i++;
    }
    return OK;
}

status modules_core_exit( void )
{
    int i = 0;

    while( core_modules[i].end_pt )
    {
        if( OK != core_modules[i].end_pt() )
        {
            err("core modules [%s] end failed\n", core_modules[i].module_name );
            return ERROR;
        }
        i++;
    }
    return OK;
}

/// @brief modules process means only need in worker process
/// @param  
/// @return 
status modules_process_init( void )
{
    int i = 0;
    while( app_modules[i].init_pt )
    {
        if( OK != app_modules[i].init_pt() )
        {
            err("modules [%s] init failed\n", app_modules[i].module_name );
            return ERROR;
        }
        i++;
    }
    return OK;
}

status modules_pocess_exit( void )
{
    int i = 0;

    while( app_modules[i].end_pt )
    {
        if( OK != app_modules[i].end_pt() )
        {
            err("modules [%s] end failed\n", app_modules[i].module_name );
            return ERROR;
        }
        i++;
    }
    return OK;
}


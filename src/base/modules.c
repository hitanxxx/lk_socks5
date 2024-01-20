#include "common.h"
#include "modules.h"
#include "dns.h"
#include "s5_server.h"
#include "s5_local.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"
#include "mailsender.h"


/// core module
// log 
// process 
// listen

/// process module
// ssl
// timer
// net
// event
// scoks5_server
// socks5_local
// http_req
// http_body
// mailsender
// webser
// dns

/// @brief core modules means need by all process include master process and worke process
/// @param  
/// @return 
status modules_core_init( void )
{
    SYS_FUNC_CHK(log_init());
    SYS_FUNC_CHK(process_init());
    SYS_FUNC_CHK(listen_init());
    return OK;
}

status modules_core_exit( void )
{
    SYS_FUNC_CHK(log_end());
    SYS_FUNC_CHK(process_end());
    SYS_FUNC_CHK(listen_end());
    return OK;
}

/// @brief modules process means only need in worker process
/// @param  
/// @return 
status modules_process_init( void )
{
    SYS_FUNC_CHK(ssl_init());
    SYS_FUNC_CHK(timer_init());
    SYS_FUNC_CHK(net_init());
    SYS_FUNC_CHK(event_init());
    SYS_FUNC_CHK(socks5_server_init());
    SYS_FUNC_CHK(socks5_local_init());
    SYS_FUNC_CHK(http_req_init_module());
    SYS_FUNC_CHK(http_body_init_module());
    SYS_FUNC_CHK(mailsender_init());
    SYS_FUNC_CHK(webser_init());
    SYS_FUNC_CHK(dns_init());
    return OK;
}

status modules_pocess_exit( void )
{
    SYS_FUNC_CHK(ssl_end());
    SYS_FUNC_CHK(timer_end());
    SYS_FUNC_CHK(net_end());
    SYS_FUNC_CHK(event_end());
    SYS_FUNC_CHK(socks5_server_end());
    SYS_FUNC_CHK(socks5_local_end());
    SYS_FUNC_CHK(http_req_end_module());
    SYS_FUNC_CHK(http_body_end_module());
    SYS_FUNC_CHK(mailsender_exit());
    SYS_FUNC_CHK(webser_end());
    SYS_FUNC_CHK(dns_end());
    return OK;
}


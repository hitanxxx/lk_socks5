#include "common.h"
#include "modules.h"
#include "dns.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"
#include "http_payload.h"
#include "http_req.h"
#include "webser.h"


/// core module
// log 
// process 
// listen

/// process module
// ssl
// timer
// net
// event
// tls_tunnel_s
// tls_tunnel_c
// http_req
// http_body
// mailsender
// webser
// dns

/// @brief core modules means need by all process include master process and worke process
/// @param  
/// @return 
int modules_core_init(void)
{
    schk(0 == log_init(), return -1);
    schk(0 == process_init(), return -1);
    schk(0 == listen_init(), return -1);
    return 0;
}

int modules_core_exit(void)
{
    schk(0 == log_end(), return -1);
    schk(0 == process_end(), return -1);
    schk(0 == listen_end(), return -1);
    return 0;
}

/// @brief modules process means only need in worker process
/// @param  
/// @return 
int modules_process_init(void)
{
    schk(0 == mem_pool_init(), return -1);
    schk(0 == ssl_init(), return -1);
    schk(0 == timer_init(), return -1);
    schk(0 == net_init(), return -1);
    schk(0 == event_init(), return -1);
    schk(0 == tls_tunnel_s_init(), return -1);
    schk(0 == tls_tunnel_c_init(), return -1);
    schk(0 == http_req_init_module(), return -1);
    schk(0 == http_payload_init_module(), return -1);
    schk(0 == webser_init(), return -1);
    schk(0 == dns_init(), return -1);
    return 0;
}

int modules_pocess_exit(void)
{
    schk(0 == ssl_end(), return -1);
    schk(0 == timer_end(), return -1);
    schk(0 == net_end(), return -1);
    schk(0 == event_end(), return -1);
    schk(0 == tls_tunnel_s_exit(), return -1);
    schk(0 == tls_tunnel_c_exit(), return -1);
    schk(0 == http_req_end_module(), return -1);
    schk(0 == http_payload_end_module(), return -1);
    schk(0 == webser_end(), return -1);
    schk(0 == dns_end(), return -1);
    schk(0 == mem_pool_deinit(), return -1);
    return 0;
}


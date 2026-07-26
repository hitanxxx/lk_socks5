#include "modules.h"
#include "common.h"
#include "dns.h"
#include "http_req.h"
#include "tls_tunnel_c.h"
#include "tls_tunnel_s.h"
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

/// @brief modules process means only need in worker process
/// @param
/// @return
int modules_process_init(void) {
    schk(0 == mem_pool_init(), return -1);
    schk(0 == ssl_init(), return -1);
    schk(0 == timer_init(), return -1);
    schk(0 == net_init(), return -1);
    schk(0 == ev_init(), return -1);
    schk(0 == tls_tunnel_s_init(), return -1);
    schk(0 == tls_tunnel_c_init(), return -1);
    schk(0 == webser_init(), return -1);
    schk(0 == dns_init(), return -1);
    return 0;
}

int modules_pocess_exit(void) {
    schk(0 == ssl_end(), return -1);
    schk(0 == timer_end(), return -1);
    schk(0 == net_end(), return -1);
    schk(0 == ev_exit(), return -1);
    schk(0 == tls_tunnel_s_exit(), return -1);
    schk(0 == tls_tunnel_c_exit(), return -1);
    schk(0 == webser_end(), return -1);
    schk(0 == dns_end(), return -1);
    schk(0 == mem_pool_deinit(), return -1);
    return 0;
}

#ifndef _NET_SSL_H_INCLUDED_
#define _NET_SSL_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

struct net_ssl_t{
    SSL_CTX *session_ctx;
    SSL *con;
    void *data;
    
    uint32_t cached_mask;
    net_ev_cb cached_readcb;
    net_ev_cb cached_writecb;

    uint8_t f_handshakeing : 1;     ///handshakeing 
    uint8_t f_handshaked : 1;       ///handshake fin
    uint8_t f_err : 1;              ///error happend. need quiet shutdown
    uint8_t f_closed : 1;           ///ssl_free immediately
} ;

int net_ssl_init(void);
int net_ssl_exit(void);


#ifdef __cplusplus
}
#endif

#endif

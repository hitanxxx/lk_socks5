#ifndef _NET_SSL_H_INCLUDED_
#define _NET_SSL_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

struct net_ssl_t{
    SSL_CTX *session_ctx;
    SSL *con;
    void *data;
    
    int cc_ev_typ;
    net_ev_cb cc_ev_cbr;
    net_ev_cb cc_ev_cbw;

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

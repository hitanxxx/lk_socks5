#ifndef _SOCKS5_H_INCLUDED_
#define _SOCKS5_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

/// s5 type for rfc reqest
#define S5_RFC_IPV4         0x1
#define S5_RFC_IPV6         0x4
#define S5_RFC_DOMAIN       0x3


typedef struct {
    unsigned char       ver;
    unsigned char       methods_n;
    unsigned char       methods_cnt;
    unsigned char       methods[255];
} s5_ph1_req_t;

typedef struct {
    unsigned char       ver;
    unsigned char       method;
} __attribute__((packed)) s5_ph1_rsp_t;

typedef struct {
    unsigned char       ver;
    unsigned char       cmd;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned char       dst_addr_n;
    unsigned char       dst_addr_cnt;
    unsigned char       dst_addr[DOMAIN_LENGTH];
    unsigned char       dst_port[2];
} s5_ph2_req_t;

typedef struct {
    unsigned char       ver;
    unsigned char       rep;
    unsigned char       rsv;
    unsigned char       atyp;
    unsigned int        bnd_addr;
    unsigned short      bnd_port;
} __attribute__((packed)) s5_ph2_rsp_t;

typedef struct {
    int s5_state;
    s5_ph1_req_t s5p1;
    s5_ph2_req_t s5p2;
} s5_t;


int s5_p1_req(event_t * ev);


#ifdef __cpluscplus
}
#endif

#endif

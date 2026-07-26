#ifndef _SOCKS5_H_INCLUDED_
#define _SOCKS5_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

/// s5 type for rfc reqest
#define S5_RFC_IPV4 0x1
#define S5_RFC_IPV6 0x4
#define S5_RFC_DOMAIN 0x3

typedef struct {
    uint8_t ver;
    uint8_t methods_n;
    uint8_t methods_cnt;
    uint8_t methods[255];
} s5_ph1_req_t;

typedef struct {
    uint8_t ver;
    uint8_t method;
} __attribute__((packed)) s5_ph1_rsp_t;

typedef struct {
    uint8_t ver;
    uint8_t cmd;
    uint8_t rsv;
    uint8_t atyp;
    uint8_t dst_addr_n;
    uint8_t dst_addr_cnt;
    uint8_t dst_addr[DOMAIN_LENGTH];
    uint8_t dst_port[2];
} s5_ph2_req_t;

typedef struct {
    uint8_t ver;
    uint8_t rep;
    uint8_t rsv;
    uint8_t atyp;
    uint8_t bnd_addr;
    uint8_t bnd_port;
} __attribute__((packed)) s5_ph2_rsp_t;

typedef struct {
    int s5_state;
    s5_ph1_req_t s5p1;
    s5_ph2_req_t s5p2;
} s5_t;

int s5_p1_req(con_t *c);

#ifdef __cpluscplus
}
#endif

#endif

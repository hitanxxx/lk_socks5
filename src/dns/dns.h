#ifndef _DNS_H_INCLUDED_
#define _DNS_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#define DNS_TMOUT 5000
#define DNS_METAN 1024  /// genernal limit 1472
#define DNS_TTL_MAX (3600 * 1000 * 3) /// 12 hours

#pragma pack(push, 1)
/// dns format in here
/// dns_header_t + qname + dns_question_t + (answer domain) + dns_rdata_t +
/// answer_addr
typedef struct dns_header {
    uint16_t id;
    uint16_t flag;
    uint16_t question_count;
    uint16_t answer_count;
    uint16_t auth_count;
    uint16_t add_count;
} dns_header_t;

typedef struct dns_question {
    /// qname
    uint16_t qtype;
    uint16_t qclass;
} dns_question_t;

typedef struct dns_rdata {
    uint16_t type;
    uint16_t rclass;
    uint32_t ttl;
    uint16_t data_len;
} dns_rdata_t;

typedef struct dns_record {
    uint8_t *name;
    dns_rdata_t *rdata;
    uint8_t *answer_addr;
} dns_record_t;
#pragma pack(pop)

typedef void (*dns_async_cb)(int status, uint8_t *res, void *data);
typedef struct  {
    uint16_t    req_id;
    ev_timer_t  *req_timer;
    char     query[DOMAIN_LENGTH]; /// stoege dns query host and convert qname

    // private
    uint32_t     qname_len;  /// question qnamelen, qname data storge in query
    dns_record_t answer; /// dns answer

    uint8_t result[16];
    dns_async_cb cb;
    void    *user_data;
} dnsc_t;


int dns_record_find(char *query, uint8_t *out_addr);

void dns_resolve_free(void *data);
void *dns_resolve(char *domain, dns_async_cb cb, void *userdata);

int dns_init(void);
int dns_end(void);

#ifdef __cplusplus
}
#endif

#endif

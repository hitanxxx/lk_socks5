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
    ev_timer_t  *   req_timer;
    uint16_t        req_transaction_id;
    uint32_t        req_qname_len;
    char            req_query[DOMAIN_LENGTH];
    uint32_t        req_query_len;

    dns_record_t    rsp_answer;
    uint8_t         rsp_result[16];

    dns_async_cb    user_cb;
    void            *user_data;

    uint8_t     finuse : 1;
    uint8_t     freserver: 7;
} dnsc_t;


int dns_resolve_free(dnsc_t *dns);
int dns_resolve(char *domain, uint32_t domain_len, dns_async_cb user_cb, void *user_data, dnsc_t *dns);


int dns_init(void);
int dns_end(void);


#ifdef __cplusplus
}
#endif

#endif

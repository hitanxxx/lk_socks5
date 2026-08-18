#include "common.h"
#include "dns.h"

typedef struct  {
    ev_timer_t *timer;
    char query[DOMAIN_LENGTH];
    uint8_t addr[4];        /// only ipv4 addr
} dns_cache_record_t;

typedef struct dns_ctx_s {
    ezhash_t *hash_record;
    ezhash_t *hash_resolve;
    con_t *c;
    uint16_t dns_id;
} dns_ctx_t;

static dns_ctx_t *dns_ctx = NULL;

static void dns_async_result(dnsc_t *dns, int result, uint8_t *ipv4) {
    if (0 != ezhash_del(dns_ctx->hash_resolve, &dns->req_id, sizeof(dns->req_id))) {
        err("dns resolve result. hash query del err\n");
    }
    if (dns->req_timer) ev_timer_free(dns->req_timer);
    if (dns->cb) dns->cb(result, ipv4, dns->user_data);
    mem_pool_free(dns);
    return;
}

static inline char *dns_get_serv(void) {
    /// try to get gateway
    if (strlen(config_get()->s5_serv_gw) > 0) {
        return config_get()->s5_serv_gw;
    } else {
        return "8.8.8.8";
    }
}

static void dns_record_timeout(ev_timer_t *timer) {
    dns_cache_record_t *record = ev_timer_userdata(timer);
    if (0 != ezhash_del(dns_ctx->hash_record, record->query, strlen(record->query))) {
        err("dns record timeout. hash record del err\n");
    }
    if (record->timer) ev_timer_free(record->timer);
    mem_pool_free(record);
}

static int dns_record_add(char *query, uint8_t *addr, uint64_t ms) {
    dns_cache_record_t *record = mem_pool_alloc(sizeof(dns_cache_record_t));

    memcpy(record->query, query, MIN(sizeof(record->query)-1, strlen(query)));
    memcpy(record->addr, addr, 4);
    if (ms > DNS_TTL_MAX) ms = DNS_TTL_MAX;
    
    ezhash_add(dns_ctx->hash_record, query, strlen(query), &record, sizeof(dns_cache_record_t *));
    record->timer = ev_timer_alloc(dns_record_timeout, record, ms);
    return 0;
}

int dns_record_find(char *query, uint8_t *out_addr) {
    dns_cache_record_t **hash_val = ezhash_find(dns_ctx->hash_record, query, strlen(query));
    if (hash_val) {
        dns_cache_record_t *record = *hash_val;
        if (record) {
            if (out_addr) memcpy(out_addr, record->addr, 4);
            return 0;
        }
    }
    return -1;
}

int dns_response_analyze(dnsc_t *dnsc) {
    uint8_t *p = NULL;
    int state_len = 0, cur = 0;
    meta_t *meta = dns_ctx->c->meta;

    enum {
        ANSWER_DOMAIN,
        ANSWER_DOMAIN2,
        ANSWER_COMMON_START,
        ANSWER_COMMON,
        ANSWER_ADDR_START,
        ANSWER_ADDR
    } state = ANSWER_DOMAIN;
    p = meta->pos + sizeof(dns_header_t) + dnsc->qname_len + sizeof(dns_question_t);
    /*
        parse dns answer
    */
    for (; p < meta->last; p++) {
        if (state == ANSWER_DOMAIN) {
            if ((*p) & 0xc0) { /// 0xc0 means two byte length
                dnsc->answer.name = p;
                state = ANSWER_DOMAIN2;
                continue;
            } else {
                /// not 0xc0 mean normal string type. then just wait the end
                /// flag 0 comes
                if (*p == 0) {
                    state = ANSWER_COMMON_START;
                    cur = 0;
                    state_len = sizeof(dns_rdata_t);
                    continue;
                }
            }
        }
        if (state == ANSWER_DOMAIN2) {
            cur = 0;
            state_len = sizeof(dns_rdata_t);
            state = ANSWER_COMMON_START;
            continue;
        }
        if (state == ANSWER_COMMON_START) {
            /// common start means common part already started
            dnsc->answer.rdata = (dns_rdata_t *)p;
            state = ANSWER_COMMON;
        }
        if (state == ANSWER_COMMON) {
            cur++;
            if (cur >= state_len) {
                /// answer common finish, goto answer address
                state = ANSWER_ADDR_START;
                cur = 0;
                state_len = ntohs(dnsc->answer.rdata->data_len);
                continue;
            }
        }
        if (state == ANSWER_ADDR_START) {
            dnsc->answer.answer_addr = p;
            state = ANSWER_ADDR;
        }
        if (state == ANSWER_ADDR) {
            cur++;
            if (cur >= state_len) {
                /// answer address finish. check address in here
                unsigned int rttl = ntohl(dnsc->answer.rdata->ttl);
                unsigned short rtyp = ntohs(dnsc->answer.rdata->type);
                unsigned short rdatan = ntohs(dnsc->answer.rdata->data_len);

                /// if this answer is a A TYPE answer (IPV4), return ok
                if (rtyp == 0x0001) {
                    if (rttl > 0 && rdatan > 0) {
                        dns_record_add((char *)dnsc->query, dnsc->answer.answer_addr, 1000 * rttl);
                    }
                    memcpy(dnsc->result, dnsc->answer.answer_addr, 4);
                    dns_async_result(dnsc, 0, dnsc->result);
                    return 0;
                } else if (rtyp == 0x0005) {
                    /// dbg("dns answer type CNAME, ignore\n");
                } else if (rtyp == 0x0002) {
                    /// dbg("dns answer type NAME SERVER, ignore\n");
                } else if (rtyp == 0x000f) {
                    /// dbg("dns answer type MAIL SERVER, ignore\n");
                }
                state = ANSWER_DOMAIN;
                cur = 0;
            }
        }
    }
    err("dns resolve rsp empty.\n");
    dns_async_result(dnsc, -1, NULL);
    return -1;
}

int dns_response_recv(con_t *c) {
    meta_t *meta = c->meta;
    meta_clr(meta);

    for (;;) {
        int ret = c->recv(c, meta->last, meta_getfree(meta));
        if (ret <= 0) {
            if (ret == 0) {
                 continue;
            } else if (ret < 0) {
                if (ret == -11) {
                    return -11;
                }
            }
        }
        
        meta->last += ret;
        /// do basic filter in here, check req question count and answer count
        dns_header_t *header = (dns_header_t *)meta->pos;
        header->id = (uint16_t)htons(header->id);
        
        dnsc_t **hash_val = ezhash_find(dns_ctx->hash_resolve, &header->id, sizeof(header->id));
        if (hash_val) {
            dnsc_t *dns = *hash_val;
            if (ntohs(header->question_count) < 1) {
                err("dns resolve rsp. question count [%d], illegal\n", header->question_count);
                dns_async_result(dns, -1, NULL);
                return -1;
            }
            if (ntohs(header->answer_count) < 1) {
                err("dns resolve rsp. answer count [%d], illegal\n", header->answer_count);
                dns_async_result(dns, -1, NULL);
                return -1;
            }
            return dns_response_analyze(dns);
        }

        meta_clr(meta);
    }
    
    return 0;
}

static int  dns_request_send(dnsc_t *dnsc) {
    meta_t *meta = dns_ctx->c->meta;

    if (meta_getlen(meta) > 0) {
        int ret = dns_ctx->c->send(dns_ctx->c, meta->pos, meta_getlen(meta));
        if (ret <= 0) {
            if (ret == -11) {
                return -11;
            }
            return -1;
        }
    }
    return 0;
}

static int dns_request_build_qname(char *host, uint8_t *qname) {
    int i = 0;
    char stack[256] = {0};
    int stackn = 0;
    int qnamen = 0;

    while (i < strlen((char *)host)) {
        if (host[i] == '.') {
            qname[qnamen++] = stackn;
            /// copy stack into qname
            memcpy(qname + qnamen, stack, stackn);
            qnamen += stackn;
            /// clear stack
            memset(stack, 0, sizeof(stack));
            stackn = 0;
        } else {
            /// push into stack
            stack[stackn++] = host[i];
        }
        i++;
    }
    /// append last part
    if (stackn > 0) {
        qname[qnamen++] = stackn;
        memcpy(qname + qnamen, stack, stackn);
        qnamen += stackn;
    }
    qname[qnamen++] = 0; /// 0 means end
    return qnamen;
}

static int dns_request_build(dnsc_t *dnsc) {
    /*
        header + question
    */
    meta_t *meta = dns_ctx->c->meta;
    meta_clr(meta);
    
    /// fill in dns packet header
    dns_header_t *header = (dns_header_t *)meta->last;
    header->id = (uint16_t)htons(dnsc->req_id);
    header->flag = htons(0x100);
    header->question_count = htons(1);
    header->answer_count = 0;
    header->auth_count = 0;
    header->add_count = 0;
    meta->last += sizeof(dns_header_t);

    /// convert www.google.com -> 3www6google3com0
    uint8_t *qname = meta->last;
    dnsc->qname_len = dns_request_build_qname(dnsc->query, qname);
    meta->last += dnsc->qname_len;

    dns_question_t *qinfo = (dns_question_t *)meta->last;
    qinfo->qtype = htons(0x0001); /// question type is IPV4
    qinfo->qclass = htons(0x0001);
    meta->last += sizeof(dns_question_t);
    return 0;
}

void dns_resolve_free(void *data) {
    dnsc_t *dns = (dnsc_t*)data;
    ezhash_del(dns_ctx->hash_resolve, &dns->req_id, sizeof(dns->req_id));
    if (dns->req_timer) ev_timer_free(dns->req_timer);
    mem_pool_free(dns);
    return;
}

static void dns_resolve_timeout(ev_timer_t *timer) {
    dnsc_t *dns = ev_timer_userdata(timer);
    err("dns resolve. timeout\n");
    dns_async_result(dns, -1, NULL);
}

void *dns_resolve(char *domain, dns_async_cb cb, void *userdata) {
    dnsc_t *dns = mem_pool_alloc(sizeof(dnsc_t));
    schk(dns, return NULL);

    dns->cb = cb;
    dns->user_data = userdata;
    memcpy(dns->query, domain, MIN(sizeof(dns->query)-1, strlen(domain)));
    dns->req_id = (uint16_t)htons((dns_ctx->dns_id    += 1) % (0xfffe));
    
    dns_request_build(dns);
    dns_request_send(dns);

    if (0 != ezhash_add(dns_ctx->hash_resolve,
        &dns->req_id, sizeof(dns->req_id), &dns, sizeof(dns))) {
        err("dns resolve. hash query add err\n");
        mem_pool_free(dns);
        return NULL;
    }
    dns->req_timer = ev_timer_alloc(dns_resolve_timeout, dns, DNS_TMOUT);
    return dns;
}

int dns_init(void) {
    if (!dns_ctx) {
        dns_ctx = mem_pool_alloc(sizeof(dns_ctx_t));
        schk(dns_ctx, return -1);
        schk(0 == ezhash_create(&dns_ctx->hash_record, 1024), return -1);
        schk(0 == ezhash_create(&dns_ctx->hash_resolve, 1024), return -1);

        schk(0 == net_alloc(&dns_ctx->c), return -1);
        dns_ctx->c->addr.sin_family = AF_INET;
        dns_ctx->c->addr.sin_port = htons(53); /// dns typicaly port: 53
        dns_ctx->c->addr.sin_addr.s_addr = inet_addr(dns_get_serv());
        schk(0 == meta_alloc(&dns_ctx->c->meta, DNS_METAN), return -1);
        schk(0 == net_connect(dns_ctx->c, NULL, 0), return -1);

        dns_ctx->c->read_cb = dns_response_recv;
        dns_ctx->c->write_cb = NULL;
        net_ev_set(dns_ctx->c, EV_R);
    }
    return 0;
}

int dns_end(void) {
    if (dns_ctx) {
        if (dns_ctx->c)  net_free(dns_ctx->c);
        if (dns_ctx->hash_record) ezhash_free(dns_ctx->hash_record);
        if (dns_ctx->hash_resolve) ezhash_free(dns_ctx->hash_resolve);
        mem_pool_free(dns_ctx);
        dns_ctx = NULL;
    }
    return 0;
}

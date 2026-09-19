#include "common.h"
#include "dns.h"

typedef struct  {
    ev_timer_t      *timer;
    char            req_query[DOMAIN_LENGTH];
    uint32_t        req_query_len;
    uint8_t         addr[4];        /// only ipv4 addr
} dns_cache_record_t;

typedef struct dns_ctx_s {
    con_t       *c;
    dnsc_t      *dns_map[0xffff];
    ezhash_t    *dns_cache;
    uint16_t    transaction_id;
} dns_ctx_t;

static dns_ctx_t *dns_ctx = NULL;


static void dns_resolve_result(dnsc_t *dns, int rsp_result, uint8_t *ipv4);

static inline char *dns_get_serv(void) {
    /// try to get gateway
    if (strlen(config_get()->s5_serv_gw) > 0) {
        return config_get()->s5_serv_gw;
    } else {
        return "8.8.8.8";
    }
}

static void dns_cache_timeout(ev_timer_t *timer) {
    dns_cache_record_t *cache = ev_timer_userdata(timer);
    
    if (0 != ezhash_del(dns_ctx->dns_cache, cache->req_query, cache->req_query_len)) {
        err("dns cache timeout. cache hash del err\n");
    }
    if (cache->timer) ev_timer_free(cache->timer);
    mem_pool_free(cache);
}

static int dns_cache_add(char *req_query, uint32_t req_query_len, uint8_t *addr, uint64_t ms) {
    dns_cache_record_t *cache = mem_pool_alloc(sizeof(dns_cache_record_t));
    if (cache) {
        cache->req_query_len = req_query_len;
        memcpy(cache->req_query, req_query, req_query_len);
        memcpy(cache->addr, addr, 4);
        if (ms > DNS_TTL_MAX) ms = DNS_TTL_MAX;

        if (0 == ezhash_add(dns_ctx->dns_cache, req_query, req_query_len, &cache, sizeof(dns_cache_record_t *))) {
            cache->timer = ev_timer_alloc_once(dns_cache_timeout, cache, ms);
        } else {
            mem_pool_free(cache);
        }
    }
    return 0;
}

static int dns_cache_find(char *req_query, uint32_t req_query_len, uint8_t *out_addr) {
    dns_cache_record_t **hash_val = ezhash_find(dns_ctx->dns_cache, req_query, req_query_len);
    if (hash_val) {
        dns_cache_record_t *cache = *hash_val;
        if (cache) {
            if (out_addr) memcpy(out_addr, cache->addr, 4);
            return 0;
        }
    }
    return -1;
}

int dns_rsp_analyze(dnsc_t *dnsc) {
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
    p = meta->pos + sizeof(dns_header_t) + dnsc->req_qname_len + sizeof(dns_question_t);
    /*
        parse dns rsp_answer
    */
    for (; p < meta->last; p++) {
        if (state == ANSWER_DOMAIN) {
            if ((*p) & 0xc0) { /// 0xc0 means two byte length
                dnsc->rsp_answer.name = p;
                state = ANSWER_DOMAIN2;
                continue;
            } else {
                /// not 0xc0 mean normal string type. then just wait the end flag 0 comes
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
            dnsc->rsp_answer.rdata = (dns_rdata_t *)p;
            state = ANSWER_COMMON;
        }
        if (state == ANSWER_COMMON) {
            cur++;
            if (cur >= state_len) {
                /// rsp_answer common finish, goto rsp_answer address
                state = ANSWER_ADDR_START;
                cur = 0;
                state_len = ntohs(dnsc->rsp_answer.rdata->data_len);
                continue;
            }
        }
        if (state == ANSWER_ADDR_START) {
            dnsc->rsp_answer.answer_addr = p;
            state = ANSWER_ADDR;
        }
        if (state == ANSWER_ADDR) {
            cur++;
            if (cur >= state_len) {
                /// rsp_answer address finish. check address in here
                unsigned int rttl = ntohl(dnsc->rsp_answer.rdata->ttl);
                unsigned short rtyp = ntohs(dnsc->rsp_answer.rdata->type);
                unsigned short rdatan = ntohs(dnsc->rsp_answer.rdata->data_len);

                /// if this rsp_answer is a A TYPE rsp_answer (IPV4), return ok
                if (rtyp == 0x0001) {
                    if (rdatan >= 4) {
                        if (rttl > 0) dns_cache_add((char *)dnsc->req_query, dnsc->req_query_len, dnsc->rsp_answer.answer_addr, 1000 * rttl);
                        memcpy(dnsc->rsp_result, dnsc->rsp_answer.answer_addr, 4);
                        dns_resolve_result(dnsc, 0, dnsc->rsp_result);
                        return 0;
                    }
                } else if (rtyp == 0x0005) {
                    /// dbg("dns rsp_answer type CNAME, ignore\n");
                } else if (rtyp == 0x0002) {
                    /// dbg("dns rsp_answer type NAME SERVER, ignore\n");
                } else if (rtyp == 0x000f) {
                    /// dbg("dns rsp_answer type MAIL SERVER, ignore\n");
                }
                state = ANSWER_DOMAIN;
                cur = 0;
            }
        }
    }
    err("dns resolve rsp empty.\n");
    dns_resolve_result(dnsc, -1, NULL);
    return -1;
}

int dns_rsp_recv(con_t *c) {
    meta_t *meta = c->meta;
    meta_clr(meta);

    for (;;) {
        int ret = c->recv(c, meta->last, meta_getfree(meta));
        if (ret <= 0) {
            if (ret == 0) {
                 return -1;
            } else if (ret < 0) {
                if (ret == -11) {
                    return -11;
                }
                return -1;
            }
        }
        
        meta->last += ret;
        
        /// do basic filter in here, check req question count and rsp_answer count
        if (meta_getlen(meta) > sizeof(dns_header_t)) {
            dns_header_t *header = (dns_header_t *)meta->pos;
            uint16_t id = (uint16_t)ntohs(header->id);
            dnsc_t *dns = dns_ctx->dns_map[id];
            if (dns) {
                if (ntohs(header->question_count) < 1) {
                    err("dns resolve rsp. question count [%d], illegal\n", header->question_count);
                    dns_resolve_result(dns, -1, NULL);
                    return -1;
                }
                if (ntohs(header->answer_count) < 1) {
                    err("dns resolve rsp. rsp_answer count [%d], illegal\n", header->answer_count);
                    dns_resolve_result(dns, -1, NULL);
                    return -1;
                }
                return dns_rsp_analyze(dns);
            }
            meta_clr(meta);
        }
    }
    
    return 0;
}

static int  dns_req_send(dnsc_t *dnsc) {
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

static int dns_req_build_qname(char *host, uint8_t *qname) {
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

static int dns_req_build(dnsc_t *dnsc) {
    /*
        header + question
    */
    meta_t *meta = dns_ctx->c->meta;
    meta_clr(meta);
    
    /// fill in dns packet header
    dns_header_t *header = (dns_header_t *)meta->last;
    header->id = (uint16_t)htons(dnsc->req_transaction_id);
    header->flag = htons(0x100);
    header->question_count = htons(1);
    header->answer_count = 0;
    header->auth_count = 0;
    header->add_count = 0;
    meta->last += sizeof(dns_header_t);

    /// convert www.google.com -> 3www6google3com0
    uint8_t *qname = meta->last;
    dnsc->req_qname_len = dns_req_build_qname(dnsc->req_query, qname);
    meta->last += dnsc->req_qname_len;

    dns_question_t *qinfo = (dns_question_t *)meta->last;
    qinfo->qtype = htons(0x0001); /// question type is IPV4
    qinfo->qclass = htons(0x0001);
    meta->last += sizeof(dns_question_t);
    return 0;
}

int dns_resolve_free(dnsc_t *dns) {
    if (dns->finuse) {
        if (dns_ctx->dns_map[dns->req_transaction_id]) 
            dns_ctx->dns_map[dns->req_transaction_id] = NULL;
        if (dns->req_timer)
            ev_timer_free(dns->req_timer);
        
        dns->finuse = 0;
    }
    return 0;
}

static void dns_resolve_result(dnsc_t *dns, int rsp_result, uint8_t *ipv4) {
    if (dns->user_cb)
        dns->user_cb(rsp_result, ipv4, dns->user_data);
    dns_resolve_free(dns);
    return;
}

static void dns_resolve_timeout(ev_timer_t *timer) {
    dnsc_t *dns = ev_timer_userdata(timer);
    err("dns resolve timeout\n");
    dns_resolve_result(dns, -1, NULL);
}

int dns_resolve(char *domain, uint32_t domain_len, dns_async_cb user_cb, void *user_data, dnsc_t *dns) {
    if (0 == dns_cache_find(domain, domain_len, dns->rsp_result)) {
        dns->finuse = 1;
        user_cb(0, dns->rsp_result, user_data);
        return 0;
    }
    
    dns->req_transaction_id = (dns_ctx->transaction_id ++) % (0xffff);
    if (!dns_ctx->dns_map[dns->req_transaction_id]) {
        dns_ctx->dns_map[dns->req_transaction_id] = dns;

        dns->finuse = 1;
        dns->req_query_len = domain_len;
        memcpy(dns->req_query, domain, domain_len);

        dns_req_build(dns);
        dns_req_send(dns);

        dns->req_timer  = ev_timer_alloc_once(dns_resolve_timeout, dns, DNS_TMOUT);
        dns->user_cb    = user_cb;
        dns->user_data  = user_data;
        return 0;
    }

    err("dns resolve. req_transaction_id already in use\n");
    return -1;
}

int dns_init(void) {
    if (!dns_ctx) {
        dns_ctx = sys_alloc(sizeof(dns_ctx_t));
        schk(dns_ctx, return -1);
        memset(dns_ctx, 0x0, sizeof(dns_ctx_t));
        schk(0 == ezhash_create(&dns_ctx->dns_cache, 1024), return -1);
        
        schk(0 == net_alloc(&dns_ctx->c), return -1);
        dns_ctx->c->addr.sin_family = AF_INET;
        dns_ctx->c->addr.sin_port = htons(53); /// dns typicaly port: 53
        dns_ctx->c->addr.sin_addr.s_addr = inet_addr(dns_get_serv());
        schk(0 == meta_alloc(&dns_ctx->c->meta, DNS_METAN), return -1);
        
        dns_ctx->c->read_cb = dns_rsp_recv;
        dns_ctx->c->write_cb = NULL;
        schk(0 == net_connect(dns_ctx->c, NULL, 0), return -1);
        net_ev_set(dns_ctx->c, EV_R);
    }
    return 0;
}

int dns_end(void) {
    if (dns_ctx) {
        if (dns_ctx->c)  
            net_free(dns_ctx->c);
        if (dns_ctx->dns_cache) 
            ezhash_free(dns_ctx->dns_cache);
        sys_free(dns_ctx);
        dns_ctx = NULL;
    }
    return 0;
}

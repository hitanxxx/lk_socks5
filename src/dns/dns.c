#include "common.h"
#include "dns.h"

#define DNS_REC_MAX    1024

typedef struct dns_cache_s {
    queue_t     queue;
    long long   expire_msec;
    char        query[DOMAIN_LENGTH+1];
    char        addr[4];
} dns_cache_t;

typedef struct dns_ctx_s
{
    int         recn;
    queue_t     record_mng;
} dns_ctx_t;
static dns_ctx_t * dns_ctx = NULL;

static int dns_async_result(dnsc_t * dns, int result_status, unsigned char * result);
static int dns_cexp(con_t * c);


static int dns_rec_add(char * query, char * addr, int msec)
{
    dns_cache_t * rdns = NULL;
    schk(rdns = mem_pool_alloc(sizeof(dns_cache_t)), return -1);
    queue_init(&rdns->queue);
    strncpy(rdns->query, query, sizeof(rdns->query));
    rdns->addr[0] = addr[0];
    rdns->addr[1] = addr[1];
    rdns->addr[2] = addr[2];
    rdns->addr[3] = addr[3];
    rdns->expire_msec = systime_msec() + msec;
    queue_insert_tail(&dns_ctx->record_mng, &rdns->queue);
    ///dbg("dns cache add entry: [%s]. ttl [%d] msec\n", query, msec);
    return 0;
}

int dns_rec_find(char * query, char * out_addr)
{   
    queue_t * q = queue_head(&dns_ctx->record_mng);
    queue_t * n = NULL;
    dns_cache_t * rdns = NULL;
    int found = 0;
    long long current_msec = systime_msec();
    
    if(queue_empty(&dns_ctx->record_mng)) {
        return -1;
    }
    while(q != queue_tail(&dns_ctx->record_mng)) {
        n = queue_next(q);
        rdns = ptr_get_struct(q, dns_cache_t, queue);

        if(current_msec < rdns->expire_msec) {
            if((strlen(rdns->query) == strlen(query)) && (!strcmp( rdns->query, query))) {  
                if(out_addr) {
                    out_addr[0] = rdns->addr[0];
                	out_addr[1] = rdns->addr[1];
                	out_addr[2] = rdns->addr[2];
                	out_addr[3] = rdns->addr[3];
                }
            	found = 1;
                if(dns_ctx->recn <= 8)
                    break;
            }
        } else {
            queue_remove(q);
            mem_pool_free(rdns);
        }
        q = n;
    }
    if(dns_ctx->recn > 8) {
        dns_ctx->recn = 0;
    }
    dns_ctx->recn++;
    return (found ? 0 : -1);
}

inline static char * dns_get_serv()
{
    /// try to get gateway 
    if(strlen(config_get()->s5_serv_gw) > 0) {
        return config_get()->s5_serv_gw;
    } else {
        return "8.8.8.8";
    }
}


int dns_response_analyze(con_t * c)
{
    unsigned char * p = NULL;
    int state_len = 0, cur = 0;
	dnsc_t * dnsc = c->data;
    meta_t * meta = c->meta;
	
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
    for(; p < meta->last; p++) {
        if(state == ANSWER_DOMAIN) {
            if((*p)&0xc0) {	/// 0xc0 means two byte length 
                dnsc->answer.name = p;
                state = ANSWER_DOMAIN2;
                continue;
            } else {
                /// not 0xc0 mean normal string type. then just wait the end flag 0 comes 
                if(*p == 0) {	
                    state = ANSWER_COMMON_START;
                    cur = 0;
                    state_len = sizeof(dns_rdata_t);
                    continue;
                }
            }
        }
        if(state == ANSWER_DOMAIN2) {
            cur = 0;
            state_len = sizeof(dns_rdata_t);
            state = ANSWER_COMMON_START;
            continue;
        }
        if(state == ANSWER_COMMON_START) {
            /// common start means common part already started
            dnsc->answer.rdata = (dns_rdata_t *)p;
            state = ANSWER_COMMON;
        }
        if(state == ANSWER_COMMON) {
            cur++;
            if(cur >= state_len) {
                ///answer common finish, goto answer address
                state = ANSWER_ADDR_START;
                cur = 0;
                state_len = ntohs(dnsc->answer.rdata->data_len);
                continue;
            }
        }
        if(state == ANSWER_ADDR_START) {
            dnsc->answer.answer_addr = p;
            state = ANSWER_ADDR;
        }
        if(state == ANSWER_ADDR) {
            cur++;
            if(cur >= state_len) {
                /// answer address finish. check address in here
                unsigned int rttl = ntohl(dnsc->answer.rdata->ttl);
                unsigned short rtyp = ntohs(dnsc->answer.rdata->type);
                unsigned short rdatan = ntohs(dnsc->answer.rdata->data_len);

                /// if this answer is a A TYPE answer (IPV4), return ok
                if(rtyp == 0x0001) {
                    if(0 == dns_rec_find((char*)dnsc->query, NULL)) {
                        /// do nothing, record already in cache 
                    } else {
                        if(rttl > 0 && rdatan > 0) {
                            dns_rec_add((char*)dnsc->query, (char*)dnsc->answer.answer_addr, 1000*rttl);
                        }
                    }
					memcpy(dnsc->result, dnsc->answer.answer_addr, 4);
					dns_async_result(dnsc, 0, dnsc->result);
                    return 0;
                } else if (rtyp == 0x0005) {
                    ///dbg("dns answer type CNAME, ignore\n");
                } else if (rtyp == 0x0002) {
                    ///dbg("dns answer type NAME SERVER, ignore\n");
                } else if (rtyp == 0x000f) {
                    ///dbg("dns answer type MAIL SERVER, ignore\n");
                }
                state = ANSWER_DOMAIN;
                cur = 0;
            }
        }
    }
    return -1;
}

int dns_response_recv(con_t * c)
{
    dnsc_t * dnsc = c->data;
    meta_t * meta = c->meta;
    dns_header_t * header = NULL;

    /// at least the response need contains the request 
    size_t want_len = sizeof(dns_header_t) + dnsc->qname_len + sizeof(dns_question_t);

    while(meta_getlen(meta) < want_len) {
        int size = udp_recvs(c, meta->last, meta_getfree(meta));
        if(size <= 0) {
            if(size == -11) {
                // add timer for recv
                tm_add(c, dns_cexp, DNS_TMOUT);
                return -11;
            }
            err("dns recv response failed, errno [%d]\n", errno);
            dns_async_result(dnsc, -1, NULL);
            return -1;
        }
        meta->last += size;
    }
    tm_del(c);

    /// do basic filter in here, check req question count and answer count     
    header = (dns_header_t*)meta->pos;
    if(ntohs(header->question_count) < 1) {
        err("dns response question count [%d], illegal\n", header->question_count);
        dns_async_result(dnsc, -1, NULL);
        return -1;
    }
    if(ntohs(header->answer_count) < 1) {
        err("dns response answer count [%d], illegal\n", header->answer_count);
        dns_async_result(dnsc, -1, NULL);
        return -1;
    }

    // make dns connection event invalidate
	return dns_response_analyze(c);
}

int dns_request_send(con_t * c)
{
    dnsc_t * dnsc = c->data;
    meta_t * meta = c->meta;

    while(meta_getlen(meta) > 0) {
        int sendn = udp_sends(c, meta->pos, meta_getlen(meta));
        if(sendn <= 0) {
            if(sendn == -11) {
                // add timer for send
                tm_add(c, dns_cexp, DNS_TMOUT);
                return -11;
            }
            err("dns send request failed, errno [%d]\n", errno);
            dns_async_result(dnsc, -1, NULL);
            return -1;
        }
        meta->pos += sendn;
    }
    tm_del(c);
    meta_clr(meta);
	
    c->ev->write_cb = NULL;
    c->ev->read_cb = dns_response_recv;
    return c->ev->read_cb(c);
}

int dns_request_host2qname(unsigned char * host, unsigned char * qname)
{
    int i = 0;
    char stack[256] = {0};
    int stackn = 0;
    int qnamen = 0;

    while(i < strlen((char*)host)) {
        if(host[i] == '.') {
            qname[qnamen++] = stackn;
            /// copy stack into qname 
            memcpy(qname+qnamen, stack, stackn);
            qnamen += stackn;
            /// clear stack
            memset(stack, 0, sizeof(stack));
            stackn = 0;
        } else {
            /// push into stack
            stack[stackn++] = host[i];
        }
        i ++;
    }
    /// append last part
    if(stackn > 0) {
        qname[qnamen++] = stackn;
        memcpy(qname+qnamen, stack, stackn);
        qnamen += stackn;
    }
    qname[qnamen++] = 0; /// 0 means end 
    return qnamen;
}


static int dns_request_packet(con_t * c)
{
    /*
        header + question
    */
 	
    dnsc_t * dnsc = c->data;
    dns_header_t * header   = NULL;
    unsigned char * qname   = NULL;
    dns_question_t * qinfo  = NULL;
    meta_t * meta = c->meta;

    /// fill in dns packet header 
    header = (dns_header_t*)meta->last;
    header->id = (unsigned short)htons(0xbeef);
    header->flag = htons(0x100);
    header->question_count = htons(1);
    header->answer_count = 0;
    header->auth_count = 0;
    header->add_count = 0;
    meta->last += sizeof(dns_header_t);

    /// convert www.google.com -> 3www6google3com0
    qname = meta->last;
    dnsc->qname_len = dns_request_host2qname(dnsc->query, qname);
    meta->last += dnsc->qname_len;

    qinfo = (dns_question_t*)meta->last;
    qinfo->qtype = htons(0x0001);  /// question type is IPV4
    qinfo->qclass = htons(0x0001);
    meta->last += sizeof(dns_question_t);

    ev_opt(c, EV_R|EV_W);
	c->ev->read_cb = NULL;
    c->ev->write_cb = dns_request_send;
    return c->ev->write_cb(c);
}



int dns_init(void)
{
    schk(!dns_ctx, return -1);
    schk(dns_ctx = mem_pool_alloc(sizeof(dns_ctx_t)), return -1);
    queue_init(&dns_ctx->record_mng);
    return 0;
}

int dns_end(void)
{
    if(dns_ctx) {
        queue_t * q = queue_head(&dns_ctx->record_mng);
        queue_t * n = NULL;
        /// free dns cache if need 
        while(q != queue_tail(&dns_ctx->record_mng)) {
            n = queue_next(q);
            
            dns_cache_t * rdns = ptr_get_struct(q, dns_cache_t, queue);
            queue_remove(q);
            mem_pool_free(rdns);

            q = n;
        }
        mem_pool_free(dns_ctx);
        dns_ctx = NULL;
    }
    return 0;
}


int dns_alloc(dnsc_t ** outdns, char * domain, dns_async_cb cb, void * userdata)
{
	dnsc_t * dns = mem_pool_alloc(sizeof(dnsc_t));
	schk(dns, return -1);

	dns->user_data = userdata;
	dns->cb = cb;
	memcpy(dns->query, domain, strlen(domain) > sizeof(dns->query) ? sizeof(dns->query) : strlen(domain));

	struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);  /// dns typicaly port: 53
    addr.sin_addr.s_addr = inet_addr(dns_get_serv());
	
	do {
		schk(0 == net_alloc(&dns->c), break);
		schk(0 == meta_alloc(&dns->c->meta, DNS_METAN), break);
		dns->c->data = dns;
		dns->c->data_cb = NULL;
		
		dns->c->fd = socket(AF_INET, SOCK_DGRAM, 0);
        schk(dns->c->fd > 0, break);
        schk(0 == net_socket_reuseaddr(dns->c->fd), break);
        schk(0 == net_socket_nbio(dns->c->fd), break);

		*outdns = dns;

        memcpy(&dns->c->addr, &addr, sizeof(dns->c->addr));
        dns->c->ev->read_cb = NULL;
        dns->c->ev->write_cb = dns_request_packet;
        return dns->c->ev->write_cb(dns->c);
	} while(0);

	dns_free(dns);
	return -1;
}

void dns_free(dnsc_t * dnsc)
{
	if(dnsc->c) net_close(dnsc->c);
	mem_pool_free(dnsc);
}

static int dns_async_result(dnsc_t * dns, int result_status, unsigned char * result)
{
	if(dns->cb)
		dns->cb(result_status, result, dns->user_data);

	return result_status == 0 ? 0 : -1;	
}

static int dns_cexp(con_t * c)
{
	dnsc_t * dns = c->data;
	return dns_async_result(dns, -1, NULL);
}

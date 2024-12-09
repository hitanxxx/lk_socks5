#ifndef _DNS_H_INCLUDED_
#define _DNS_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define DNS_TMOUT   5000
#define DNS_METAN    1500

#pragma pack(push,1)
/// dns format in here
/// dns_header_t + qname + dns_question_t + (answer domain) + dns_rdata_t + answer_addr
typedef struct dns_header {
    unsigned short  id;
    unsigned short  flag;
    unsigned short  question_count;
    unsigned short  answer_count;
    unsigned short  auth_count;
    unsigned short  add_count;
}  dns_header_t;

typedef struct dns_question {    
    /// qname 
    unsigned short  qtype;
    unsigned short  qclass;
}  dns_question_t;

typedef struct dns_rdata {
    unsigned short  type;
    unsigned short  rclass;
    uint32          ttl;
    unsigned short  data_len;
}  dns_rdata_t;

typedef struct dns_record {
    unsigned char * name;
    dns_rdata_t *   rdata;
     unsigned char *    answer_addr;   
}  dns_record_t;
#pragma pack(pop)

typedef void (* dns_async_cb)(int status, unsigned char * res, void * data);
typedef struct dnsc {
    // in && out
    unsigned char   query[DOMAIN_LENGTH+1];    /// stoege dns query host and convert qnam e
    con_t *  c;
    
    dns_async_cb       cb;
    void *          user_data;
    
    //int              result_status; // OK:success     -1:error
    unsigned char     result[16];
    
    // private
    uint32          qname_len;    /// question qnamelen, qname data storge in query 
    dns_record_t    answer;    /// dns answer
} dnsc_t;

void dns_free(dnsc_t * dnsc);
int dns_alloc(dnsc_t ** dns, char * domain, dns_async_cb cb, void * userdata);
int dns_request_host2qname(unsigned char * host, unsigned char * qname);


int dns_init(void);
int dns_end(void);

int dns_rec_find(char * query, char * out_addr);

    
#ifdef __cplusplus
}
#endif
    
#endif


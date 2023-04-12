#ifndef _DNS_H_INCLUDED_
#define _DNS_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define DNS_TIMEOUT         5
#define DNS_BUFFER_LEN      1500

#pragma pack(push,1)
typedef struct dns_header 
{
    unsigned short  id;
    unsigned short  flag;
    unsigned short  question_count;
    unsigned short  answer_count;
    unsigned short  auth_count;
    unsigned short  add_count;
}  dns_header_t;

typedef struct dns_question
{	
    unsigned short  qtype;
    unsigned short  qclass;
}  dns_question_t;

typedef struct dns_rdata
{
    unsigned short  type;
    unsigned short  rclass;
    uint32          ttl;
    unsigned short  data_len;

    unsigned char   data[0];
}  dns_rdata_t;

typedef struct dns_record
{
    unsigned char * name;
    dns_rdata_t *   rdata;
    unsigned char * data;
}  dns_record_t;
#pragma pack(pop)

typedef void ( * dns_callback )( void * data );
typedef struct dns_cycle
{
    queue_t         queue;
    // in && out
    unsigned char   query[DOMAIN_LENGTH+1];
    connection_t *  c;
    dns_callback    cb;
    void *          cb_data;
    status          dns_status; // OK:success 	ERROR:erro

	// private
    uint32          qname_len;
    dns_record_t    answer;
    
    meta_t          dns_meta;
    unsigned char   dns_buffer[DNS_BUFFER_LEN];
} dns_cycle_t;

status l_dns_start( dns_cycle_t * cycle );
status l_dns_create( dns_cycle_t ** dns_cycle );
status l_dns_over( dns_cycle_t * cycle );
uint32_t l_dns_request_qname_conv( unsigned char * qname, unsigned char * query );

status l_dns_init( void );
status l_dns_end( void );
    
#ifdef __cplusplus
}
#endif
    
#endif


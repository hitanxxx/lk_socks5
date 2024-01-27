#ifndef _DNS_H_INCLUDED_
#define _DNS_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


#define DNS_TIMEOUT         5
#define DNS_BUFFER_LEN      1500

#pragma pack(push,1)
/// dns format in here
/// dns_header_t + qname + dns_question_t + (answer domain) + dns_rdata_t + answer_addr
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
	/// qname 
    unsigned short  qtype;
    unsigned short  qclass;
}  dns_question_t;

typedef struct dns_rdata
{
    unsigned short  type;
    unsigned short  rclass;
    uint32          ttl;
    unsigned short  data_len;
}  dns_rdata_t;

typedef struct dns_record
{
    unsigned char * name;
    dns_rdata_t *   rdata;
 	unsigned char *	answer_addr;   
}  dns_record_t;
#pragma pack(pop)

typedef void ( * dns_callback )( void * data );
typedef struct dns_cycle
{
    queue_t         queue;
    // in && out
    unsigned char   query[DOMAIN_LENGTH+1];	/// stoege dns query host and convert qnam e
    connection_t *  c;
    dns_callback    cb;
    void *          cb_data;
    status          dns_status; // OK:success 	ERROR:error
    
	meta_t          dns_meta;
    unsigned char   dns_buffer[DNS_BUFFER_LEN];
	
	// private
    uint32          qname_len;	/// question qnamelen, qname data storge in query 
    dns_record_t    answer;	/// dns answer
} dns_cycle_t;

status dns_start( dns_cycle_t * cycle );
status dns_create( dns_cycle_t ** dns_cycle );
status dns_over( dns_cycle_t * cycle );
int dns_request_host2qname( unsigned char * host, unsigned char * qname );


status dns_init( void );
status dns_end( void );

status dns_rec_find( char * query, char * out_addr );

    
#ifdef __cplusplus
}
#endif
    
#endif


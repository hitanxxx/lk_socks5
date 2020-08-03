#ifndef _L_DNS_H_INCLUDED_
#define _L_DNS_H_INCLUDED_

#define DNS_TIMEOUT 5

typedef struct dns_header 
{
	unsigned short 	id;
	unsigned short  flag;
	unsigned short  question_count;
	unsigned short  answer_count;
	unsigned short  auth_count;
	unsigned short  add_count;
} __attribute__((packed)) dns_header_t;

typedef struct dns_question
{	
	unsigned short  qtype;
	unsigned short  qclass;
} __attribute__((packed)) dns_question_t;


typedef struct dns_rdata
{
	unsigned short  type;
	unsigned short  class;
	uint32   ttl;
	unsigned short  data_len;

	unsigned char 	data[0];
} __attribute__((packed)) dns_rdata_t;

typedef struct dns_record
{
	unsigned char * name;
	dns_rdata_t *   rdata;
	unsigned char * data;
} __attribute__((packed)) dns_record_t;

typedef void ( * dns_callback )( void * data );
typedef struct dns_cycle
{
	dns_callback	cb;
	void * 			cb_data;

	status			dns_status; // OK:success 	ERROR:erro

	unsigned char 	query[64];
	unsigned char	dns_serv[64];

	connection_t 	* c;

	uint32			qname_len;
	dns_record_t 	answer;
} dns_cycle_t;

char * l_dns_get_serv( );
status l_dns_start( dns_cycle_t * cycle );
status l_dns_create( dns_cycle_t ** dns_cycle );
status l_dns_free( dns_cycle_t * cycle );


#endif


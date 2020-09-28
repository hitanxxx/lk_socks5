#ifndef _L_HTTP_RESPONSE_HEAD_H_INCLUDED_
#define _L_HTTP_RESPONSE_HEAD_H_INCLUDED_

typedef struct http_response_head_t http_response_head_t;
typedef status (*response_handler)( http_response_head_t * );
typedef struct http_response_handlers 
{
	response_handler					process;
	response_handler 					exit;
} http_response_handlers_t;

struct http_response_head_t {
	queue_t								queue;
	uint32								state;
	connection_t * 						c;
	http_response_handlers_t 			handler;

	string_t 							header_key;
	string_t 							header_value;

	string_t 							http_code;
	string_t							http_version;
	string_t 							http_string;
	uint32								http_status_code;

	uint32								body_type;
	uint32								content_length;
	uint32 								keepalive;

	meta_t 								head;
};

status http_response_head_create( connection_t * c, http_response_head_t ** response );
status http_response_head_init_module ( void );
status http_response_head_end_module( void );

#endif

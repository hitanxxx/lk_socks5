#ifndef _L_HTTP_REQUEST_HEAD_H_INCLUDED_
#define _L_HTTP_REQUEST_HEAD_H_INCLUDED_

#define REQ_LENGTH_URI_STR			2048


typedef struct http_request_head_t http_request_head_t;
typedef status ( * header_handler ) ( http_request_head_t * request, string_t * str, uint32 offsetof );
struct header_t 
{
	string_t 			name;
	header_handler		handler;
	uint32				offsetof;
} header_t;




typedef struct request_headers_in_t {
	mem_list_t 			*list;

	string_t *			host;
	string_t * 			connection;
	string_t * 			content_length;
	string_t *			range;
	string_t * 			if_modified_since;
	string_t *			referer;
	string_t * 			transfer_encoding;
	string_t *			user_agent;
} request_headers_in_t;


typedef status ( *request_handler ) ( http_request_head_t * request );
typedef struct http_request_handers_t
{
	request_handler			process;
	request_handler			exit;
} http_request_handers_t;

struct http_request_head_t 
{
	queue_t				queue;
	connection_t * 		c;
	uint32 				state;
	http_request_handers_t	handler;

	char *				request_line_start;
	char * 				request_line_end;
	string_t			method;
	string_t			schema;
	string_t 			host;
	string_t 			uri;
	string_t 			port;
	string_t 			http_version;

	string_t 			header_key;
	string_t 			header_value;
	request_headers_in_t	headers_in;

	uint32			keepalive_flag;
	uint32			body_type;
	uint32			content_length;
};


status http_request_head_init_module( void );
status http_request_head_end_module( void );

status http_request_head_create( connection_t * c, http_request_head_t ** request );

#endif

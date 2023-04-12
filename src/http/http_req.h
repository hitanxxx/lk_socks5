#ifndef _HTTP_REQ_H_INCLUDED_
#define _HTTP_REQ_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define REQ_LENGTH_URI_STR			2048

typedef struct http_req http_req_t;
typedef status ( *http_req_header_cb ) ( http_req_t * request, string_t * str, uint32 offsetof );
typedef struct
{
    string_t           		name;
    http_req_header_cb      handler;
    uint32              	offsetof;
} http_req_value_t;

typedef struct
{
    mem_arr_t*         list;
	string_t*           host;
	string_t*           connection;
	string_t*           content_length;
	string_t*           range;
	string_t*           if_modified_since;
	string_t*           referer;
	string_t*           transfer_encoding;
	string_t*           user_agent;
} http_req_headers_t;


typedef status ( *http_req_cb ) ( http_req_t * request );

struct http_req
{
    queue_t                     queue;
    connection_t *              c;
    uint32                      state;
    http_req_cb					cb;

	// request line
	unsigned char*              request_line_start;
	unsigned char*              request_line_end;
	
	string_t                    method;
	string_t                    schema;
	string_t                    host;
	string_t                    uri;
	string_t                    port;
	string_t                    http_version;

	// req headers
	string_t                    header_key;
	string_t                    header_value;
	http_req_headers_t       	headers;

	// referenced by extern
	uint32                      keepalive;
	uint32                      content_type;
	uint32                      content_length;
};

status http_req_have_body(     http_req_t * req);
status http_req_create( connection_t * c, http_req_t ** req );
status http_req_free( http_req_t * req );
status http_req_init_module( void );
status http_req_end_module( void );
    
#ifdef __cplusplus
}
#endif
        
    
#endif

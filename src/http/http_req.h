#ifndef _HTTP_REQ_H_INCLUDED_
#define _HTTP_REQ_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define REQ_LENGTH_URI_STR			2048

typedef struct http_req http_req_t;
typedef status (*http_req_header_cb) (http_req_t * request, string_t * str, unsigned int offsetof);
typedef struct {
    string_t           		name;
    http_req_header_cb      handler;
    uint32              	offsetof;
} http_req_value_t;


typedef status (*http_req_cb )(http_req_t * request);
struct http_req {
    con_t *         c;
    http_req_cb     cb;
    uint32          state;

    enum http_process_status  method_typ;
	string_t    method;
	string_t    scheme;
	string_t    host;
	string_t    uri;
	string_t    port;
	string_t    http_ver;

	///headers
	string_t  k;
	string_t  v;
	string_t    headerhost;
	string_t    connection;
	string_t    content_length;
	string_t    range;
	string_t    if_modified_since;
	string_t    referer;
	string_t    transfer_encoding;
	string_t    user_agent;

	// user for outer 
	char  fkeepalive:1;
	int  payload_typ;
	int  payload_contentn;
};

int http_req_ctx_init(con_t * c, http_req_t ** req);
int http_req_ctx_exit(http_req_t * req);
int http_req_init_module(void);
int http_req_end_module(void);
    
#ifdef __cplusplus
}
#endif
        
    
#endif

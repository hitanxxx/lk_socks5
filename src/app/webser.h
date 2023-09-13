#ifndef _WEBSER_H_INCLUDED_
#define _WEBSER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define WEBSER_TIMEOUT 5
#define WEBSER_LENGTH_INDEX_STR 32
#define WEBSER_LENGTH_HOME_STR 256
#define WEBSER_REQ_META_LEN 4096
#define WEBSER_BODY_META_LENGTH 32768
#define WEBSER_LENGTH_PATH_STR (REQ_LENGTH_URI_STR + WEBSER_LENGTH_HOME_STR + WEBSER_LENGTH_INDEX_STR)


typedef struct
{
    char str[128];
} webser_resp_head_line_t;

typedef struct webser_t
{
    queue_t queue;
    connection_t *c;
    void *data;
    int type;

    http_req_t *http_req;
    http_body_t *http_req_body;

    // api data
    event_pt webapi_handler;

    // static file data
    int ffd;
    int fsize;
    int fsend;

    int             http_rsp_headern;
    meta_t *        http_rsp_header_meta;
    mem_list_t *    http_rsp_header_list;
    
    int             http_rsp_bodyn;
    meta_t *        http_rsp_body_meta;
    mem_list_t *    http_rsp_body_list;
    
    int             http_rsp_code;
    char *          http_rsp_mime;
	
} webser_t;

typedef struct mime_type_t
{
    string_t type;
    string_t header;
} mime_type_t;

void webser_rsp_body_push_str( webser_t * webser, char * str );
void webser_rsp_mime(webser_t *webser, char *mimetype);

status webser_process_req_body(event_t *ev);
status webser_process_req_webapi(event_t *ev);

status webser_init(void);
status webser_end(void);

status webser_accept_cb(event_t *ev);
status webser_accept_cb_ssl(event_t *ev);

status webser_api_reg( char * key, event_pt cb );

#ifdef __cplusplus
}
#endif

#endif

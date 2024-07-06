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
#define WEBSER_BODY_META_LENGTH 16384
#define WEBSER_LENGTH_PATH_STR (REQ_LENGTH_URI_STR + WEBSER_LENGTH_HOME_STR + WEBSER_LENGTH_INDEX_STR)

typedef struct {
	char key[32];
	event_pt  handler;
    enum http_process_status  method;
    unsigned char body_need;
    unsigned char auth_need;
} webser_api_t;

typedef struct {
    con_t *c;
    void *data;
    int type;

    http_req_t *http_req;
    http_payload_t *http_payload;

    // static file data
    int ffd;        // file fd  
    int fsize;      // file allsize
    int fsend;      // file sendsize

    // api data
    webser_api_t * api;

	int             http_rsp_code;
    char *          http_rsp_mime;	


	meta_t * rsp_meta_header;
	meta_t * rsp_meta_body;
} webser_t;


void webser_rsp_mime(webser_t *webser, char *mimetype);
void webser_timeout_cycle(void * data);
int webser_req_body_proc(webser_t * web);
int webser_free(webser_t * webser);
int webser_rsp_send(event_t * ev);
int webser_rsp_code(webser_t * webser, int http_code);
int webser_rsp_body_push(webser_t * webser, const char * str, ...);

int webser_init(void);
int webser_end(void);

int webser_accept_cb(event_t *ev);
int webser_accept_cb_ssl(event_t *ev);
int webser_api_reg(char * key, event_pt cb, enum http_process_status method_type, char body_need);


#ifdef __cplusplus
}
#endif

#endif

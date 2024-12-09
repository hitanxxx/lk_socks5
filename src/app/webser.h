#ifndef _WEBSER_H_INCLUDED_
#define _WEBSER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define WEB_TMOUT 5*1000
#define WEB_PAYLOAD_LEN 16384

typedef struct {
	char key[32];
	ev_cb  handler;
    enum http_process_status  method;
    unsigned char body_need;
    unsigned char auth_need;
} webser_api_t;

typedef struct {
    con_t *c;
    int type;

    web_req_t * webreq;
   	
    // static file data
    int ffd;        // file fd  
    int fsize;      // file allsize
    int fsend;      // file sendsize

    // api data
    webser_api_t * api;

	meta_t * rsp_hdr;
	meta_t * rsp_payload;
} webser_t;


int webser_rsp(con_t * c, int rcode, char * exthdr, void * payload, int payloadn);

int webser_init(void);
int webser_end(void);

int webser_accept_cb(con_t * c);
int webser_accept_cb_ssl(con_t * c);
int webser_api_reg(char * key, ev_cb cb, enum http_process_status method_type, char body_need);


#ifdef __cplusplus
}
#endif

#endif

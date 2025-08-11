#ifndef _HTTP_REQ_H_INCLUDED_
#define _HTTP_REQ_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define REQ_KV_MAX  16

typedef struct web_req web_req_t;
typedef int (*web_req_cb) (con_t * c, web_req_t * req);

typedef struct web_kv {
    string_t k;
    string_t v;
} web_kv_t;

struct web_req {
    web_req_cb cb;

    int state;
    int method_typ;
    string_t method;
    string_t scheme;
    string_t host;
    string_t uri;
    string_t port;
    string_t http_ver;

    int kvn;
    web_kv_t kvs[REQ_KV_MAX];
    
    char fkeepalive:1;
    int payloadn;
    meta_t * payload;
};


web_req_t * web_req_alloc(void);
void web_req_free(web_req_t * webreq);


    
#ifdef __cplusplus
}
#endif
        
    
#endif

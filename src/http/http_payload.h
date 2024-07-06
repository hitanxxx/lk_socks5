#ifndef _HTTP_PAYLOAD_H_INCLUDED_
#define _HTTP_PAYLOAD_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define CHUNK_HEXN 16
#define HTTP_PAYLOAD_BUFN  4096

typedef struct http_payload http_payload_t;
typedef status (*http_payload_cb) (http_payload_t * pl);
struct http_payload {
    queue_t  queue;
    con_t  *c;
    int state;
    http_payload_cb cb;
    meta_t * meta;
    char fcache:1;
    char fchunk:1;

    int ilen;
    int in;
    
	unsigned char hex[CHUNK_HEXN];
	int hexn;

    meta_t * payload;
    int payloadn;
};


int http_payload_init_module(void);
int http_payload_end_module(void);
int http_payload_ctx_exit(http_payload_t *ctx);
int http_payload_ctx_init(con_t * c, http_payload_t ** ctx, int enable_cache);

#ifdef __cplusplus
}
#endif
        

#endif

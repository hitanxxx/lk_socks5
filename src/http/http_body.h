#ifndef _HTTP_BODY_H_INCLUDED_
#define _HTTP_BODY_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH     20
#define ENTITY_BODY_BUFFER_SIZE                     8192

typedef struct http_body http_body_t;
typedef status (*http_body_cb) (http_body_t * bd);
struct http_body 
{
    queue_t  queue;
    con_t  *c;
    int state;
    http_body_cb cb;

    ///[IN]data.
	int body_type;		/// body is content or chunk 
	int body_cache;		/// body cache or not save (just recvd)

    ///[PRIVATE]content
    int content_len;
	int	content_recvd;
    	
    ///[PRIVATE]chunk
	unsigned char hex_str[ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH];
	int hex_len;
	unsigned char * chunk_pos;
    int chunk_part_cur;
    int chunk_part_len;
	meta_t* chunk_meta;

    ///[OUT] data.
	int body_status;	/// body recv status (finish or not)
    ///storge final body data
	meta_t * body_head;
	meta_t * body_last;
	int body_len;

    meta_t * body_dump;
};

int http_body_dump(http_body_t * bd);
int http_body_create(con_t * c, http_body_t ** body, int discard);
int http_body_free(http_body_t * bd);
int http_body_init_module(void);
int http_body_end_module(void);

#ifdef __cplusplus
}
#endif
        

#endif

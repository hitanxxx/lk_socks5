#ifndef _HTTP_BODY_H_INCLUDED_
#define _HTTP_BODY_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH     20
#define ENTITY_BODY_BUFFER_SIZE                     8192

typedef struct http_body http_body_t;
typedef status ( *http_body_cb ) ( http_body_t * bd );
struct http_body 
{
    queue_t                 queue;
    connection_t            *c;
    uint32                  state;
    http_body_cb    		cb;

	uint32                  body_type;		/// body is content or chunk 
	uint32                  body_cache;		/// body cache or not save (just recvd)
	status					body_status;	/// body current status 
   
    int             content_len;
	int				content_recvd;
	

	unsigned char           hex_str[ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH];
	uint32                  hex_len;
	
	unsigned char *         chunk_pos;
    uint32                  chunk_part_cur;
    uint32                  chunk_part_len;
	meta_t* 				chunk_meta;
    
	meta_t *                body_head;
	meta_t *                body_last;
	uint32                  body_len;
};

status http_body_create( connection_t * c, http_body_t ** body, int discard );
status http_body_free( http_body_t * bd );
status http_body_init_module( void );
status http_body_end_module( void );

#ifdef __cplusplus
}
#endif
        

#endif

#ifndef _L_HTTP_ENTITYBODY_H_INCLUDED_
#define _L_HTTP_ENTITYBODY_H_INCLUDED_

#define ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH		20
#define ENTITY_BODY_BUFFER_SIZE 	8192

typedef struct http_body_t http_body_t;
typedef status ( *http_body_handler ) ( http_body_t * bd );

typedef struct http_body_handlers_t
{
	http_body_handler	process;
	http_body_handler	exit;
} http_body_handlers_t;

struct http_body_t 
{
	queue_t					queue;
	connection_t 			*c;
	uint32					state;
	http_body_handlers_t	handler;

	// in
	uint32 					body_type;
	uint32					body_cache;
	event_pt				callback;
	// out
	int						callback_status;
	meta_t * 				body_head;
	meta_t *				body_last;
	uint32					body_length;

	//private content
	uint32					content_length;
	uint32					content_need;
	//private chunk
	char *					chunk_pos;
	uint32					chunk_part_cur;
	uint32	 				chunk_part_len;
	uint32					chunk_all_length;

	char					hex_str[ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH];
	uint32					hex_length;
};


status http_body_create( connection_t * c, http_body_t ** body, int discard );
status http_body_init_module( void );
status http_body_end_module( void );


#endif

#ifndef _L_HTTP_ENTITYBODY_H_INCLUDED_
#define _L_HTTP_ENTITYBODY_H_INCLUDED_

#define ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH		20
#define ENTITY_BODY_BUFFER_SIZE 	8192

typedef struct http_entitybody_t http_entitybody_t;
typedef status ( *http_body_handler ) ( http_entitybody_t * bd );

typedef struct http_body_handlers_t
{
	http_body_handler	process;
	http_body_handler	exit;
	
} http_body_handlers_t;

struct http_entitybody_t {
	queue_t				queue;
	connection_t * 		c;
	uint32				state;
	http_body_handlers_t	handler;

	uint32 				body_type;
	uint32				cache;


	// for over callback use
	event_pt			over_cb;
	int					status;

	/*
	 * private infos
	 */
	// content infos
	uint32				content_length;
	uint32				content_need;
	// chunked infos
	uint32 				chunk_length;
	uint32				chunk_recvd;
	uint32	 			chunk_need;
	char *				chunk_pos;
	uint32				chunk_all_length;

	char				hex_str[ENTITY_BODY_CHUNKED_PART_HEX_STR_LENGTH];
	uint32				hex_length;
	// data chain
	meta_t * 			body_head;
	meta_t *			body_last;

	uint32				all_length;
};


status http_entitybody_init_module( void );
status http_entitybody_end_module( void );

status http_entitybody_create( connection_t * c, http_entitybody_t ** body, int discard );

#endif

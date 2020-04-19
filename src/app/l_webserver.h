#ifndef _L_WEBSERVER_H_INCLUDED_
#define _L_WEBSERVER_H_INCLUDED_

#define WEBSER_TIMEOUT					5
#define WEBSER_LENGTH_INDEX_STR			32
#define WEBSER_LENGTH_HOME_STR			256
#define WEBSER_LENGTH_PATH_STR \
( REQ_LENGTH_URI_STR + WEBSER_LENGTH_HOME_STR + WEBSER_LENGTH_INDEX_STR )
#define WEBSER_BODY_META_LENGTH		32768

typedef struct webser_t
{
	queue_t 			queue;
	connection_t 		*c;
	void * 				data;

	http_request_head_t *	request_head;
	http_entitybody_t* 		request_body;

	uint32				api_flag;
	serv_api_handler	api_handler;

	uint32 				filesize;
	uint32 				re_status;
	char				filepath[WEBSER_LENGTH_PATH_STR];

	int32				ffd;
	meta_t				*response_head;
	meta_t				*response_body;

	//upstream_t * 		upstream;
} webser_t;

typedef struct mime_type_t
{
	string_t		type;
	string_t		header;
} mime_type_t;

status webser_set_mimetype( webser_t * webserwebser, char * str, uint32 length );
status webser_entity_head( webser_t * webser );
status webser_response( event_t * ev );
status webser_over( webser_t * webser );

status webser_init( void );
status webser_end( void );

status webser_process_init( void );
status webser_process_end( void );

#endif

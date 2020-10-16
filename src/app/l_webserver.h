#ifndef _L_WEBSERVER_H_INCLUDED_
#define _L_WEBSERVER_H_INCLUDED_

#define WEBSER_TIMEOUT                  5
#define WEBSER_LENGTH_INDEX_STR         32
#define WEBSER_LENGTH_HOME_STR          256
#define WEBSER_REQ_META_LEN             4096
#define WEBSER_BODY_META_LENGTH         32768
#define WEBSER_LENGTH_PATH_STR \
( REQ_LENGTH_URI_STR + WEBSER_LENGTH_HOME_STR + WEBSER_LENGTH_INDEX_STR )

typedef struct webser_t
{
    queue_t                 queue;
    connection_t *          c;
    void*                   data;
    int32					type;
    
    http_req_head_t*        http_req_head;
    http_body_t*            http_resp_body;
    uint32                  http_resp_code;

    // api data
    serv_api_handler        webapi_handler;

    // static file data
    int32                   filefd;
    string_t*               file_mime;
    ssize_t                 filelen, filesend;

    meta_t*                 response_head;
    meta_t*                 response_body;
} webser_t;

typedef struct mime_type_t
{
	string_t		type;
	string_t		header;
} mime_type_t;

status webser_process_req_body( event_t * ev );

void webser_interface_set_body_len( webser_t * webser, uint32 body_len );
void webser_interface_set_mimetype( webser_t * webser, char * mimetype );

status webser_init( void );
status webser_end( void );

#endif

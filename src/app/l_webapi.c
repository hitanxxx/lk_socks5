#include "l_base.h"
#include "l_http_body.h"
#include "l_http_request_head.h"
#include "l_http_response_head.h"
#include "l_webserver.h"


static status webapi_hello_world( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
    meta_t * meta = NULL;
    
    // get request body
    if( (NULL == webser->http_resp_body) || !(webser->http_resp_body->callback_status & HTTP_BODY_STAT_OK) )
    {
        // static will discard all http request body
        if( OK != http_body_create( webser->c, &webser->http_resp_body, 1 ) )
        {
            err("http_body_create failed\n");
            return ERROR;
        }
        webser->http_resp_body->body_type           = webser->http_req_head->content_type;
        webser->http_resp_body->content_length      = webser->http_req_head->content_length;
        webser->http_resp_body->callback            = webapi_hello_world;
        
        c->event->read_pt = webser_process_req_body;
        return c->event->read_pt( ev );
    }
    
    // build response body
    if( OK != meta_page_alloc( c->page, 64, &webser->response_body ) )
    {
        err("web api [hello world] meta alloc failed\n");
        return ERROR;
    }
    meta = webser->response_body;
    strcpy( (char*)meta->last, "hello world" );
    meta->last += l_strlen("hello world");
    
    webser_interface_set_body_len( webser, meta_len( meta->pos, meta->last ) );
    webser_interface_set_mimetype( webser, ".html" );
    return OK;
}

status webapi_init(  )
{
    serv_api_register( "/helloworld", webapi_hello_world );
	return OK;
}


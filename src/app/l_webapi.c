#include "l_base.h"
#include "l_http_body.h"
#include "l_http_request_head.h"
#include "l_http_response_head.h"
#include "l_webserver.h"
#include "l_mailsender.h"

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

static status webapi_mailsender( event_t * ev )
{
	int rc = OK;
	int idx = 0;
	ljson_ctx_t * json_ctx = NULL; 
	mailsender_t * mailsender = NULL;
	
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	meta_t * meta = NULL;

	/*
		create detached thread for mail sender,
		because sender email need send and recv 
		many times
	*/
	// get request body
    if( (NULL == webser->http_resp_body) || !(webser->http_resp_body->callback_status & HTTP_BODY_STAT_OK) )
    {
        // static will discard all http request body
        if( OK != http_body_create( webser->c, &webser->http_resp_body, 1 ) )
        {
            err("webapi mailsender, http_body_create failed\n");
            return ERROR;
        }
		webser->http_resp_body->body_cache  		= 1;
        webser->http_resp_body->body_type           = webser->http_req_head->content_type;
        webser->http_resp_body->content_length      = webser->http_req_head->content_length;
        webser->http_resp_body->callback            = webapi_mailsender;
        
        c->event->read_pt = webser_process_req_body;
        return c->event->read_pt( ev );
    }

	// get request body data
	if( OK != meta_page_alloc( c->page, webser->http_resp_body->body_length, &meta ) )
	{
		err("webapi mailsender, alloc meta container for request body failed\n");
		return ERROR;
	}
	meta_t * local_meta = webser->http_resp_body->body_head;
	while( local_meta != NULL )
	{
		memcpy( meta->last, local_meta->pos, meta_len( local_meta->pos, local_meta->last ) );
		meta->last += meta_len( local_meta->pos, local_meta->last );
		local_meta = local_meta->next;
	}

	if( OK != mailsender_alloc( &mailsender ) )
	{
		err("webapi mailsender, alloc mailsender failed\n");
		return ERROR;
	}

	// get sender infomation
	do 
	{
		if( OK != json_ctx_create( &json_ctx ) )
	    {
	        err("webapi mailsender, start json ctx create\n" );
	        rc = ERROR;
			break;
	    }
		if( OK != json_decode( json_ctx, meta->pos, meta->last ) )
	    {
	        err("webapi mailsender, configuration file json decode failed\n" );
	       	rc = ERROR;
			break;
	    }
		ljson_node_t *root_obj, *v;

		json_get_child( &json_ctx->root, 1, &root_obj );

		if( OK != json_get_obj_bool( root_obj, "tls", strlen("tls"), &v) )
		{
			err("webapi mailsender, find 'tls' failed\n");
			rc = ERROR;
			break;
		}
		mailsender->tls = ( v->node_type == JSON_TRUE ) ? 1 : 0;
		
		if( OK != json_get_obj_str( root_obj, "host", strlen("host"), &v) )
		{
			err("webapi mailsender, find 'host' failed\n");
			rc = ERROR;
			break;
		}
		memcpy( mailsender->host, v->name.data, v->name.len );
		
		if( OK != json_get_obj_num( root_obj, "port", strlen("port"), &v ) )
		{
			err("webapi mailsender, find 'port' failed\n");
			rc = ERROR;
			break;
		}
		mailsender->port = v->num_i;
		
		if( OK != json_get_obj_str( root_obj, "host_username", strlen("host_username"), &v ) )
		{
			err("webapi mailsender, find 'host_username' failed\n");
			rc = ERROR;
			break;
		}
		memcpy( mailsender->host_username, v->name.data, v->name.len );

		if( OK != json_get_obj_str( root_obj, "host_passwd", strlen("host_passwd"), &v ) )
		{
			err("webapi mailsender, find 'host_passwd' failed\n");
			rc = ERROR;
			break;
		}
		memcpy( mailsender->host_passwd, v->name.data, v->name.len );

		if( OK != json_get_obj_str( root_obj, "from", strlen("from"), &v ) )
		{
			err("webapi mailsender, find 'from' failed\n");
			rc = ERROR;
			break;
		}
		memcpy( mailsender->from, v->name.data, v->name.len );

		if( OK != json_get_obj_str( root_obj, "to", strlen("to"), &v ) )
		{
			err("webapi mailsender, find 'to' failed\n");
			rc = ERROR;
			break;
		}
		memcpy( mailsender->to, v->name.data, v->name.len );

		if( OK != json_get_obj_str( root_obj, "regcode", strlen("regcode"), &v ) )
		{
			err("webapi mailsender, find 'regcode' failed\n");
			rc = ERROR;
			break;
		}
		snprintf( mailsender->context, sizeof(mailsender->context),
			"Your regcode is %.*s", v->name.len, v->name.data
		);
		memcpy( mailsender->subject, "Alexa Regcode", strlen("Alexa Regcode") );
	}while(0);

	if( json_ctx )
	{
		json_ctx_free( json_ctx );
	}

	if( OK != mailsender_send( mailsender ) )
	{
		err("webapi mailsender, send failed\n");
		rc = ERROR;
	}
	return rc;
}

status webapi_init(  )
{
    serv_api_register( "/helloworld", webapi_hello_world );
	serv_api_register( "/mailsender", webapi_mailsender );
	return OK;
}


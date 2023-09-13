#include "common.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"
#include "mailsender.h"


static status webapi_hello_world( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;

	cJSON * root = cJSON_CreateObject();
	if( root ) {
		cJSON_AddStringToObject( root, "hello", "world" );
		char * json_rsp = cJSON_Print(root);
		if( json_rsp ) {
			webser_rsp_body_push_str( webser, json_rsp );
			webser_rsp_mime( webser, ".html" );
			cJSON_free(json_rsp);
		}
		cJSON_Delete(root);
		return 200;
	}
	return 400;
}

status webapi_init(  )
{
    webser_api_reg( "/helloworld", webapi_hello_world );
	return OK;
}


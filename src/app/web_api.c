#include "common.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"
#include "mailsender.h"

static status wapi_echo_post( event_t * ev )
{
    /// example for do post api function
    connection_t * c = ev->data;
    webser_t * web = c->data;

    /// dump http req body into single meta (meta_http_req_body)
    meta_t * meta_http_req_body = NULL;
    if( OK != http_body_dump( web->http_req_body, &meta_http_req_body ) ) {
        err("dump http req body failed\n");
        return 500;
    }
    /// process the http req body
    debug("http req data [%s]\n", meta_http_req_body->pos );
    

    /// build the http rsp body datas
    cJSON *root = cJSON_CreateObject();
    if( root ) {
        cJSON_AddStringToObject( root, "data", (char*)meta_http_req_body->pos );
        char * cjson_str = cJSON_PrintUnformatted(root);
        if( cjson_str ) {
            webser_rsp_mime( web, ".html" );
            webser_rsp_body_push_str( web, cjson_str );
            cJSON_free(cjson_str);
        }
        cJSON_Delete(root);
        return 200;
    }
    return 400;
}

static status wapi_echo_get( event_t * ev )
{   
    /// example for do get api function 
    connection_t * c = ev->data;
    webser_t * web = c->data;

    /// build the http rsp body datas
    cJSON *root = cJSON_CreateObject();
    if( root ) {
        cJSON_AddStringToObject( root, "data", "echo" );
        char * cjson_str = cJSON_PrintUnformatted(root);
        if( cjson_str ) {
            webser_rsp_mime( web, ".html" );
            webser_rsp_body_push_str(web, cjson_str );
            cJSON_free(cjson_str);
        }
        cJSON_Delete(root);
        return 200;
    }
    return 400;
}

status webapi_init(  )
{   
    
    webser_api_reg("/echo", wapi_echo_get, HTTP_METHOD_GET, 0 );
    webser_api_reg("/echo", wapi_echo_post, HTTP_METHOD_POST, 1 );
    return OK;
}


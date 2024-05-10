#include "common.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"

static status wapi_echo_post( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * web = c->data;
    int http_code = 0;
    
    int rc = webser_req_body_proc(web );
    if(rc == ERROR) {
        err("web req body proc failed\n");
        webser_free(web);
        return ERROR;
    } else if (rc == AGAIN) {
        timer_set_data( &ev->timer, web );
        timer_set_pt( &ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

    ///show req body
    dbg("http req data [%s]\n", web->http_req_body->body_dump->pos );

    webser_rsp_mime( web, ".html" );
    webser_rsp_body_push( web, (char*)web->http_req_body->body_dump->pos );
    http_code = 200;

    
    if(OK != webser_rsp_body_dump(web)) {
        err("webser rsp body build failed\n");
        webser_free(web);
        return ERROR;
    }
    if( OK != webser_req_header_dump( web, http_code ) ) {
        err("webser rsp header build failed\n");
        webser_free(web);
        return ERROR;
    }
    ev->read_pt = webser_rsp_send;
    return ev->read_pt( ev );
}

static status wapi_echo_get( event_t * ev )
{   
    con_t * c = ev->data;
    webser_t * web = c->data;
    int http_code = 0;

    /// build the http rsp body datas
    webser_rsp_mime( web, ".html" );
    webser_rsp_body_push( web, "Hello World!!! %s", systime_gmt() );
    http_code = 200;

    
    if(OK != webser_rsp_body_dump(web)) {
        err("webser rsp body build failed\n");
        webser_free(web);
        return ERROR;
    }
    if( OK != webser_req_header_dump( web, http_code ) ) {
        err("webser rsp header build failed\n");
        webser_free(web);
        return ERROR;
    }
    ev->read_pt = webser_rsp_send;
    return ev->read_pt( ev );
}

status webapi_init(  )
{   
    webser_api_reg("/echo", wapi_echo_get, HTTP_METHOD_GET, 0 );
    webser_api_reg("/echo", wapi_echo_post, HTTP_METHOD_POST, 1 );
    return OK;
}


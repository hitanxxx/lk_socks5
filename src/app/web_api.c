#include "common.h"
#include "http_payload.h"
#include "http_req.h"
#include "webser.h"

static int wapi_echo_post(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * web = c->data;
    
    int rc = webser_req_body_proc(web);
    if(rc == -1) {
        err("web req body proc failed\n");
        webser_free(web);
        return -1;
    } else if (rc == -11) {
        timer_set_data(&ev->timer, web);
        timer_set_pt(&ev->timer, webser_timeout_cycle);
        timer_add(&ev->timer, WEBSER_TIMEOUT);
        return -11;
    }
    timer_del(&ev->timer);

    ///show req body
    dbg("http req data [%s]\n", web->http_payload->payload->pos);

    webser_rsp_mime(web, ".html");
	webser_rsp_code(web, 200);
    schk(0 == webser_rsp_body_push(web, (char*)web->http_payload->payload->pos), return -1);
    schk(0 == webser_rsp_body_push(web, systime_gmt()), return -1);
    
    ev->read_pt = webser_rsp_send;
    return ev->read_pt( ev );
}

static int wapi_echo_get(event_t * ev)
{   
    con_t * c = ev->data;
    webser_t * web = c->data;

    /// build the http rsp body datas
    webser_rsp_mime(web, ".html");
    webser_rsp_code(web, 200);
    schk(0 == webser_rsp_body_push(web, "Hello World!!! %s", systime_gmt()), return -1);
    
    ev->read_pt = webser_rsp_send;
    return ev->read_pt(ev);
}

int webapi_init(  )
{   
    webser_api_reg("/echo", wapi_echo_get, HTTP_METHOD_GET, 0);
    webser_api_reg("/echo", wapi_echo_post, HTTP_METHOD_POST, 1);
    return OK;
}


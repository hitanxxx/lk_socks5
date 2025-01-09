#include "common.h"
#include "http_req.h"
#include "webser.h"

static int wapi_echo_post(con_t * c)
{
    webser_t * webc = c->data;

    ///dbg("http req data [%s]\n", webc->webreq->payload->pos);
    return webser_rsp(c, 200, NULL, webc->webreq->payload->pos, meta_getlen(webc->webreq->payload));
}

static int wapi_echo_get(con_t * c)
{   
    return webser_rsp(c, 200, NULL, systime_gmt(), strlen(systime_gmt()));
}

int webapi_init(  )
{   
    webser_api_reg("/echo", wapi_echo_get, HTTP_METHOD_GET, 0);
    webser_api_reg("/echo", wapi_echo_post, HTTP_METHOD_POST, 1);
    return OK;
}


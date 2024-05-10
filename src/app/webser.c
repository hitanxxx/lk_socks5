#include "common.h"
#include "web_api.h"
#include "dns.h"
#include "s5_server.h"
#include "http_req.h"
#include "http_body.h"
#include "webser.h"


typedef struct
{
    mem_arr_t * g_api_list;
    ezhash_t * mime_hash;
} g_web_t;
static g_web_t * g_web_ctx = NULL;


static status webser_start( event_t * ev );

static status webser_api_find( webser_t * web )
{
    uint32 i = 0;
    webser_api_t *api_entry = NULL;

    if( !g_web_ctx->g_api_list ) {
        return ERROR;
    }
    for( i = 0; i < g_web_ctx->g_api_list->elem_num; i ++ ) {
        api_entry = mem_arr_get( g_web_ctx->g_api_list, i + 1 );
        if( api_entry == NULL ) {
            continue;
        }

        /// compare key 
        if( strlen(api_entry->key) != web->http_req->uri.len ) {
            continue;
        }
        if( 0 != strncmp( api_entry->key, (char*)web->http_req->uri.data, web->http_req->uri.len ) ) {
            continue;
        }
        /// compare method
        if( web->http_req->method_type != api_entry->method ) {
            continue;
        }
        web->api = api_entry;
        return OK;
    }
    return ERROR;
}

status webser_api_reg( char * key, event_pt cb, enum http_process_status method_type, char http_req_body_need )
{
    webser_api_t * p = mem_arr_push( g_web_ctx->g_api_list );
    if( !p ) {
        err("webser g_api_list mem list push failed\n");
        return ERROR;
    }
    int keyn = 0;
    if( strlen(key) > (sizeof(p->key)-1) ) {
        keyn = sizeof(key)-1;
    } else {
        keyn = strlen(key);
    }
    strncpy( p->key, key, keyn );
    
    p->handler = cb;
    p->method = method_type;
    p->body_need = http_req_body_need;
    return OK;
}

static status webser_alloc( webser_t ** webser )
{
    webser_t * nweb = mem_pool_alloc( sizeof(webser_t) );
    if(!nweb) {
        err("nweb alloc failed\n");
        return ERROR;
    }
    *webser = nweb;
    return OK;
}

static status webser_over( webser_t * webser )
{
    /// different with webser_free -> not free connection
    /// used for http keepalive. free webser but not free conection

    webser->data = NULL;
    webser->type = 0;

    meta_t * m = NULL;
    meta_t * n = NULL;

    m = webser->http_rsp_header_meta;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    webser->http_rsp_header_meta = NULL;

    m = webser->http_rsp_body_meta;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    webser->http_rsp_body_meta = NULL;
    
    mem_list_free( webser->http_rsp_header_list );
    mem_list_free( webser->http_rsp_body_list );
    webser->http_rsp_body_list = NULL;
    webser->http_rsp_header_list = NULL;

    
    if( webser->http_req ) {
        http_req_free( webser->http_req );
        webser->http_req = NULL;
    }
    if( webser->http_req_body ) {
        http_body_free( webser->http_req_body );
        webser->http_req_body = NULL;
    }
    webser->api = NULL;
    
    if( webser->ffd ) {
        close( webser->ffd );
        webser->ffd = 0;
    }
    webser->fsize = 0;
    webser->fsend = 0;

    webser->http_rsp_mime = NULL;
    webser->http_rsp_bodyn = 0;
    webser->http_rsp_code = 0;

    mem_pool_free(webser);
    return OK;
}

status webser_free( webser_t * webser )
{
    net_free( webser->c );

    webser->data = NULL;
    webser->type = 0;
    
    mem_list_free( webser->http_rsp_header_list );
    mem_list_free( webser->http_rsp_body_list );
    webser->http_rsp_body_list = NULL;
    webser->http_rsp_header_list = NULL;
    
    if( webser->http_req ) {
        http_req_free( webser->http_req );
        webser->http_req = NULL;
    }
    if( webser->http_req_body ) {
        http_body_free( webser->http_req_body );
        webser->http_req_body = NULL;
    }
    webser->api = NULL;
    
    if( webser->ffd ) {
        close( webser->ffd );
        webser->ffd = 0;
    }
    webser->fsize = 0;
    webser->fsend = 0;

    webser->http_rsp_mime = NULL;
    webser->http_rsp_bodyn = 0;
    webser->http_rsp_code = 0;

    meta_t * m = webser->http_rsp_header_meta;
    meta_t * n = NULL;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    webser->http_rsp_header_meta = NULL;

    m = webser->http_rsp_body_meta;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    webser->http_rsp_body_meta = NULL;

    mem_pool_free(webser);
    return OK;
}

inline static void webser_timeout_con( void * data )
{
    net_free( (con_t*)data );
}

void webser_timeout_cycle( void * data )
{
    webser_free( (webser_t*)data );
}

static status webser_try_read( event_t * ev  )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    int n = 0;
    char buf[1] = {0};

    n = recv(c->fd, buf, 1, MSG_PEEK);
    if (n == 0)  {
        /// do nothing
    } else if (n == -1) {
        if (errno != AGAIN) {
            goto closed;
        }
    }
    return OK;
closed:
    err("webser try read failed\n");
    webser_free( webser );
    return ERROR;
}

static status webser_keepalive( event_t * ev )
{
    con_t* c = ev->data;
    webser_t * webser = c->data;
    uint32 remain = meta_len( c->meta->pos, c->meta->last );

    if(remain) {
        memcpy( c->meta->start, c->meta->pos, remain );
    }
    c->meta->pos    = c->meta->start;
    c->meta->last   = c->meta->pos + remain;
    ///free web cycle. but reuse net connection
    webser_over( webser );

    ev->write_pt = NULL;
    ev->read_pt = webser_start;
    event_opt( ev, c->fd, EV_R );
    return ev->read_pt( ev );
}


static int webser_rsp_mime_init( )
{
    if( 0 != ezhash_create( &g_web_ctx->mime_hash, 128 ) ) {
        err("webser create mime hash failed\n");
        return ERROR;
    }    
    
    ezhash_add( g_web_ctx->mime_hash, ".html",  "Content-type: text/html\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".js",    "Content-type: application/x-javascript\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".json",  "Content-type: application/json\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".png",   "Content-type: image/png\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".jpg",   "Content-type: image/jpeg\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".jpeg",  "Content-type: image/jpeg\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".gif",   "Content-type: image/gif\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".ico",   "Content-type: image/x-icon\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".css",   "Content-type: text/css\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".txt",   "Content-type: text/plain\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".htm",   "Content-type: text/html\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".mp3",   "Content-type: audio/mpeg\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".m3u8",  "Content-type: application/x-mpegURL\r\n" );
    ezhash_add( g_web_ctx->mime_hash, ".ts",    "Content-type: video/MP2T\r\n" );
    return OK;
}

static char* webser_rsp_mime_find( unsigned char * str, int len )
{
    if( str == NULL && len == 0 ) {
        return "Content-type: application/octet-stream\r\n";
    } else {
        return ezhash_find( g_web_ctx->mime_hash, (char*)str );
    }
}


void webser_rsp_mime( webser_t * webser, char * mimetype )
{
    webser->http_rsp_mime = webser_rsp_mime_find( (unsigned char*)mimetype, l_strlen(mimetype) );
}

static status webser_rsp_send_body_api( event_t * ev )
{	
    con_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;
    
    /// send http rsp payload string 
    if( webser->http_rsp_body_meta ) {
        rc = c->send_chain( c, webser->http_rsp_body_meta );
        if( rc < 0 )  {
            if( rc == ERROR ) {
                err("webser send resp body failed\n");
                webser_free( webser );
                return ERROR;
            }
            timer_set_data( &ev->timer, webser );
            timer_set_pt( &ev->timer, webser_timeout_cycle );
            timer_add( &ev->timer, WEBSER_TIMEOUT );
            return AGAIN;
        }
    }
    timer_del( &ev->timer );
    
    if( webser->http_rsp_code == 200 && webser->http_req->keepalive ) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    }

    webser_free( webser );
    return OK;
}

static status webser_rsp_send_body_file( event_t * ev )
{	
    con_t * c = ev->data;
    webser_t * webser = c->data;
    meta_t * meta = webser->http_rsp_body_meta;

    for(;;) {
        if( meta->last > meta->pos ) {
            int sendn = c->send( c, meta->pos, meta->last - meta->pos );
            if( sendn < 0 ) {
                if( sendn == ERROR ) {
                    err("webser file content send error\n");
                    webser_free(webser);
                    return ERROR;
                }
                timer_add( &ev->timer, WEBSER_TIMEOUT );
                return AGAIN;
            }
            meta->pos += sendn;
            webser->fsend += sendn;
            if(webser->fsend >= webser->fsize) {
                break;
            }
        }
        
        if( meta->last == meta->pos )
            meta->last = meta->pos = meta->start;
        
        if(meta->last < meta->end) {
            int readn = read(webser->ffd, meta->last, meta->end - meta->last );
            if( readn <= 0 ) {
                err("webser read file, errno [%d]\n", errno );
                webser_free( webser );
                return ERROR;
            }
            meta->last += readn;
        }
    }
    timer_del( &ev->timer );
    
    if( webser->http_rsp_code == 200 && webser->http_req->keepalive ) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    } 
    webser_free(webser);
    return OK;
}


static status webser_rsp_send_body( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    /// send http rsp payload
    if( webser->type == WEBSER_FILE ) {
        /// for mem pool alloc sign 
        if( OK != meta_alloc(&webser->http_rsp_body_meta, WEBSER_BODY_META_LENGTH-sizeof(meta_t) ) ) {
            err("webser rsp body meta alloc failed\n");
            webser_free(webser);
            return ERROR;
        }
        ev->write_pt = webser_rsp_send_body_file;
        return ev->write_pt( ev );
    } else {
        /// http rsp payload type string 
        ev->write_pt = webser_rsp_send_body_api;
        return ev->write_pt( ev );
    }
}

static status webser_rsp_send_header( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    /// send http rsp header
    rc = c->send_chain( c, webser->http_rsp_header_meta );
    if( rc < 0 ) {
        if( rc == ERROR ) {
            err("webser resp head send failed\n");
            webser_free( webser );
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt( &ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

    /// if no http rsp payload
    if( webser->http_rsp_bodyn <= 0 ) {
        if( webser->http_rsp_code == 200 && webser->http_req->keepalive == 1 ) {
            ev->write_pt = webser_keepalive;
            return ev->write_pt( ev );
        }
        webser_free( webser );
        return OK;
    }

    ev->write_pt = webser_rsp_send_body;
    return ev->write_pt( ev );
}


status webser_rsp_send( event_t * ev )
{
    con_t * c = ev->data;
    ev->read_pt = webser_try_read;
    ev->write_pt = webser_rsp_send_header;
    event_opt( ev, c->fd, EV_R|EV_W );
    return ev->write_pt( ev );
}

/// @brief for easy add http rsp payload string 
/// @param webser 
/// @param str 
void webser_rsp_body_push( webser_t * webser, const char * str, ... )
{
    va_list argslist;
    const char * args = str;
    char buf[1024] = {0};

    va_start( argslist, str );
    vsnprintf( buf, sizeof(buf)-1, args, argslist );
    va_end( argslist );

    if( 0 != mem_list_push( &webser->http_rsp_body_list, buf ) ) {
        err("webser rsp body list push str [%s] failed\n", buf );
    }
}


status webser_rsp_body_dump( webser_t *webser)
{
    webser->http_rsp_bodyn = 0;
    mem_list_t * cur = webser->http_rsp_body_list;
    while( cur ) {
        webser->http_rsp_bodyn += cur->datan;
        cur = cur->next;
    }

    /// get http rsp payload data
    if( webser->http_rsp_bodyn > 0 ) {
        if( OK != meta_alloc( &webser->http_rsp_body_meta, webser->http_rsp_bodyn ) ) {
            err("webser alloc rsp body meta failed\n");
            webser_free(webser);
            return ERROR;
        }
        cur = webser->http_rsp_body_list;
        while( cur ) {
            memcpy( webser->http_rsp_body_meta->last, cur->data, cur->datan );
            webser->http_rsp_body_meta->last += cur->datan;
            cur = cur->next;
        }
    }
    return OK;
}

/// @brief for easy add http rsp header string 
/// @param webser 
/// @param str 
void webser_rsp_header_push( webser_t * webser, const char * str, ... ) 
{
    va_list argslist;
    const char * args = str;
    char buf[1024] = {0};
    
    va_start( argslist, str );
    vsnprintf( buf, sizeof(buf)-1, args, argslist );
    va_end( argslist );
    
    if( 0 != mem_list_push( &webser->http_rsp_header_list, buf) ) {
        err("webser rsp header list push str [%s] failed\n", buf );
    }
}

status webser_req_header_dump( webser_t * webser, int http_code )
{
    webser->http_rsp_code = http_code;
    switch( http_code ) {
        case 200:
            webser_rsp_header_push(webser, "HTTP/1.1 200 OK\r\n" );
            break;
        case 400:
            webser_rsp_header_push(webser, "HTTP/1.1 400 Bad Request\r\n" );
            break;
        case 404:
            webser_rsp_header_push(webser, "HTTP/1.1 404 Not Found\r\n" );
            break;
        case 500:
            webser_rsp_header_push(webser, "HTTP/1.1 500 Internal Server Error\r\n" );
            break;
        case 403:
            webser_rsp_header_push(webser, "HTTP/1.1 403 Forbidden\r\n" );
            break;
        default:
            webser_rsp_header_push(webser, "HTTP/1.1 400 Bad Request\r\n" );
            break;
    }
    webser_rsp_header_push(webser, "Server: SSSSS\r\n" );
    webser_rsp_header_push(webser, "Accept-Charset: utf-8\r\n" );
    webser_rsp_header_push(webser, "Date: %s\r\n", systime_gmt() );
    webser_rsp_header_push(webser, "Content-Length: %d\r\n", webser->http_rsp_bodyn );
    webser_rsp_header_push(webser, webser->http_rsp_mime ? webser->http_rsp_mime : "Content-type: application/octet-stream\r\n" );
    if( webser->http_req->headers.connection && ((uint32)webser->http_req->headers.connection->len > l_strlen("close")) ) {
        webser_rsp_header_push( webser, "Connection: keep-alive\r\n" );
    } else {
        webser_rsp_header_push( webser, "Connection: close\r\n" );
    }
    webser_rsp_header_push( webser, "\r\n" );

    
    mem_list_t * cur = webser->http_rsp_header_list; /// get http rsp header len  
    while( cur ) {
        webser->http_rsp_headern += cur->datan;
        cur = cur->next;
    }
    if( webser->http_rsp_headern > 0 ) { /// get http rsp header data
        if( OK != meta_alloc( &webser->http_rsp_header_meta, webser->http_rsp_headern ) ) {
            err("webser alloc rsp header meta failed\n");
            webser_free(webser);
            return ERROR;
        }
        cur = webser->http_rsp_header_list;
        while( cur ) {
            memcpy( webser->http_rsp_header_meta->last, cur->data, cur->datan );
            webser->http_rsp_header_meta->last += cur->datan;
            cur = cur->next;
        }
    }
    return OK;
}

static status webser_req_file ( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    int http_code = 400;
    char path[WEBSER_LENGTH_PATH_STR] = {0};

    char * p = path;
    int pn = 0;
    
    pn += snprintf( p+pn, WEBSER_LENGTH_PATH_STR-pn, "%s", config_get()->http_home );
    if( webser->http_req->uri.len > 0 ) {
        pn += snprintf( p+pn, WEBSER_LENGTH_PATH_STR-pn, "%.*s", webser->http_req->uri.len, webser->http_req->uri.data );
        // if url is a dir, need to add index path
        if( webser->http_req->uri.data[webser->http_req->uri.len-1] == '/' ) {
            pn += snprintf( p+pn, WEBSER_LENGTH_PATH_STR-pn, "%s", config_get()->http_index );
        }
    } else {
        pn += snprintf( p+pn, WEBSER_LENGTH_PATH_STR-pn, "/%s", config_get()->http_index );
    }
    
    char * postfix = strrchr( path, '.' );
    if( postfix ) {
        webser->http_rsp_mime = webser_rsp_mime_find( (unsigned char*)postfix, strlen(postfix) );
    } else {
        webser->http_rsp_mime = webser_rsp_mime_find( (unsigned char*)NULL, 0 ); 
    }

    webser->fsize = webser->fsend = 0;
    struct stat st;
    if( OK != stat( path, &st ) ) {
        err("webser stat check file [%s] failed, [%d]\n", path, errno );
        http_code = 400;
    } else {
        if(st.st_mode & S_IFMT) {
            if(st.st_mode & S_IRUSR) {
                webser->ffd = open( path, O_RDONLY );
                if( -1 == webser->ffd ) {
                    err("webser open static file failed [%s], errno [%d]\n", path, errno );
                    http_code = 500;
                }
                http_code = 200;
                webser->fsize = st.st_size;
            } else {
                http_code = 403;
            }
        } else {
            http_code = 404;
        }
    }
    webser->http_rsp_bodyn = webser->fsize;
    if( OK != webser_req_header_dump( webser, http_code ) ) {
        err("webser resp head build failed\n");
        webser_free( webser );
        return ERROR;
    }
    
    ev->read_pt = webser_rsp_send;
    return ev->read_pt( ev );
}

status webser_req_api( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    return webser->api->handler( ev );
}

static status webser_req_router( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    /// static file request or api request  
    rc = webser_api_find( webser );
    if( rc == OK ) {
        webser->type = WEBSER_API;
        ev->read_pt = webser_req_api; 
    } else {
        ///static request only support GET method
        if( webser->http_req->method_type != HTTP_METHOD_GET ) {
            err("webser static req. method not support\n");
            webser_free(webser);
            return ERROR;
        }
        
        webser->type = WEBSER_FILE;
        ev->read_pt = webser_req_file;
    }
    return ev->read_pt( ev );
}

status webser_req_body_proc( webser_t * web )
{
    if(web->http_req->content_type == HTTP_BODY_TYPE_NULL) return DONE;
    if(!web->http_req_body) {
        if( OK != http_body_create( web->c, &web->http_req_body, (web->api->body_need == 1) ? 0 : 1 ) ) {
            err("http_body_create failed\n");
            return ERROR;
        }
        web->http_req_body->body_type = web->http_req->content_type;
        if( web->http_req_body->body_type == HTTP_BODY_TYPE_CONTENT )
            web->http_req_body->content_len = web->http_req->content_len;
    }

    int rc = web->http_req_body->cb( web->http_req_body );
    if( rc == ERROR ) {
        err("webser process body failed\n");
        return rc;
    } else if (rc == AGAIN) {
        return AGAIN;
    } 
    ///dump body
    if( OK != http_body_dump(web->http_req_body) ) {
        err("webser req body dump failed\n");
        return ERROR;
    }
    return DONE;
}


static status webser_req_header_proc( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    /// goto parse http request line and headers
    rc = webser->http_req->cb( webser->http_req );
    if( rc < 0 ) {
        if( rc == ERROR ) {
            err("webser process request header failed\n");
            webser_free( webser );
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt(&ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

    /// if don't have body, excute down 
    ev->read_pt	= webser_req_router;
    return ev->read_pt( ev );
}

static status webser_start( event_t * ev )
{
    con_t * c = ev->data;
    webser_t * webser = NULL;

    // init web connection page and meta memory for use
    
    if(!c->meta) {
        if( OK != meta_alloc( &c->meta, 4096 ) ) {
            err("webser con meta alloc failed\n");
            net_free(c);
            return ERROR;
        }
    }
    
    if( OK != webser_alloc( &webser ) ) {
        err("webser con webser alloc failed\n");
        net_free(c);
        return ERROR;
    }
    webser->c = c;
    c->data = webser;

    /// start http request parse, try to read http request form socket and parse it 
    if( OK != http_req_create( c, &webser->http_req ) ) {
        err("webser alloc req failed\n");
        webser_free( webser );
        return ERROR;
    }
    ev->write_pt = NULL;
    ev->read_pt = webser_req_header_proc;
    return ev->read_pt( ev );
}

/// @brief  transfer request to s5 module 
/// @param ev 
/// @return 
static status webser_transfer_to_s5( event_t * ev )
{
    con_t * c = ev->data;
    meta_t * meta = NULL;
    ssize_t rc = 0;
    
    // page && meta init
   
    if(!c->meta) {
        if( OK != meta_alloc( &c->meta, 8192 ) ) {
            err("webser alloc con meta failed\n");
            net_free(c);
            return ERROR;
        }
    }

    /// try to recv s5 private authorization header 
    meta = c->meta;
    while( meta_len( meta->pos, meta->last ) < sizeof(s5_auth_t) ) {
        rc = c->recv( c, meta->last, meta_len( meta->last, meta->end ) );
        if( rc < 0 ) {
            if( ERROR == rc ) {
                err("webser procotol route recv failed\n");
                net_free( c );
                return ERROR;
            }
            timer_set_data( &ev->timer, c );
            timer_set_pt(&ev->timer, webser_timeout_con );
            timer_add( &ev->timer, WEBSER_TIMEOUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );
    
    /// s5 private authorization header data check 
    do {
        s5_auth_t * header = (s5_auth_t*)meta->pos;
        if( S5_AUTH_LOCAL_MAGIC != ntohl( header->magic ) ) {
            // magic number not match, goto http/https process
            break;
        }
        // magic number match success, goto s5 process
        ev->read_pt = s5_server_transport;
        return ev->read_pt( ev );
    } while(0);

    /// match failure, go back to webserver process
    ev->read_pt = webser_start;
    return ev->read_pt( ev );
}

static status webser_accept_cb_ssl_check( event_t * ev )
{
    con_t * c = ev->data;

    if( !c->ssl->handshaked ) {
        err("webser handshake failed\n");
        net_free( c );
        return ERROR;
    }
    timer_del( &c->event->timer );

    c->recv = ssl_read;
    c->send = ssl_write;
    c->recv_chain = NULL;
    c->send_chain = ssl_write_chain;

    ev->write_pt = NULL;
    if( config_get()->s5_mode == SOCKS5_SERVER_SECRET ) {
        /// s5 secret mode. use https listen port accept request
        ev->read_pt = webser_transfer_to_s5;
    } else {
        ev->read_pt = webser_start;
    }
    return ev->read_pt( ev );
}



status webser_accept_cb_ssl( event_t * ev )
{
    con_t * c = ev->data;
    status rc = 0;

    do {
        rc = net_check_ssl_valid(c);
        if( OK != rc ) {
            if( AGAIN == rc ) {
                timer_set_data( &ev->timer, c );
                timer_set_pt( &ev->timer, webser_timeout_con );
                timer_add( &ev->timer, WEBSER_TIMEOUT );
                return AGAIN;
            }
            err("webser check net ssl failed\n");
            break;
        }
        
        if( OK != ssl_create_connection( c, L_SSL_SERVER ) ) {
            err("webser ssl con create failed\n");
            break;
        }
        rc = ssl_handshake( c->ssl );
        if( rc < 0 ) {
            if( rc == ERROR ) {
                err("webser ssl handshake failed\n");
                break;
            }
            c->ssl->cb = webser_accept_cb_ssl_check;
            timer_set_data( &ev->timer, c );
            timer_set_pt( &ev->timer, webser_timeout_con );
            timer_add( &ev->timer, WEBSER_TIMEOUT );
            return AGAIN;
        }
        return webser_accept_cb_ssl_check( ev );
    } while(0);

    net_free( c );
    return ERROR;
}

status webser_accept_cb( event_t * ev )
{
    ev->read_pt = webser_start;
    return ev->read_pt( ev );
}


/// @brief webserver module init 
/// @param  
/// @return 
status webser_init( void )
{
    int ret = -1;

    if( g_web_ctx ) {
        err("g_web_ctx not empty\n");
        return ERROR;
    }

    do {
        g_web_ctx = mem_pool_alloc(sizeof(g_web_t) );
        if( !g_web_ctx ) {
            err("webser alloc this failed, [%d]\n", errno );
            return ERROR;
        }
        
        /// init mime hash 
        if( OK != webser_rsp_mime_init( )) {
            mem_pool_free(g_web_ctx);
            err("webser mime hash init failed\n");
            return ERROR;
        }
        
        /// init webserv api data
        if( OK != mem_arr_create( &g_web_ctx->g_api_list, sizeof(webser_api_t) ) ) {
            err("webser create api mem arr create failed\n" );
            break;
        }
        
        webapi_init();
        ret = 0;
    } while(0);

    if( 0 != ret ) {
        if( g_web_ctx ) {
            if( g_web_ctx->g_api_list ) {
                mem_arr_free( g_web_ctx->g_api_list );
                g_web_ctx->g_api_list = NULL;
            }
            mem_pool_free(g_web_ctx);
            g_web_ctx = 0;
        }
    }
    return (0 == ret)? OK:ERROR;
}

status webser_end( void )
{
    if( g_web_ctx ) {
        if( g_web_ctx->mime_hash ) {
            ezhash_free( g_web_ctx->mime_hash );
        }
        if( g_web_ctx->g_api_list ) {
            mem_arr_free( g_web_ctx->g_api_list );
            g_web_ctx->g_api_list = NULL;
        }
        mem_pool_free( g_web_ctx );
    }
    return OK;
}

#include "common.h"
#include "web_api.h"
#include "dns.h"
#include "s5_server.h"
#include "http_req.h"
#include "http_body.h"
#include "webser.h"

typedef struct 
{
	string_t 			key;
	event_pt			handler;
} webser_api_t;

typedef struct
{
    mem_arr_t *  g_api_list;
    mem_page_t * g_api_page;

    queue_t     g_queue_use;
    queue_t     g_queue_usable;
    webser_t    g_pool[0];
} g_web_t;
static g_web_t * g_web_ctx = NULL;

static mime_type_t mimetype_table[] =
{
    {string(".*"),				string("Content-type: application/octet-stream\r\n")},
    {string(".html"),			string("Content-type: text/html\r\n")},
    {string(".js"),             string("Content-type: application/x-javascript\r\n")},
    {string(".json"),			string("Content-type: application/json\r\n")},
    {string(".png"),			string("Content-type: image/png\r\n")},
    {string(".jpg"),			string("Content-type: image/jpeg\r\n")},
    {string(".jpeg"),			string("Content-type: image/jpeg\r\n")},
    {string(".gif"),			string("Content-type: image/gif\r\n")},
    {string(".ico"),			string("Content-type: image/x-icon\r\n")},
    {string(".css"),			string("Content-type: text/css\r\n")},
    {string(".txt"),			string("Content-type: text/plain\r\n")},
    {string(".htm"),			string("Content-type: text/html\r\n")},
    {string(".mp3"),			string("Content-type: audio/mpeg\r\n")},
    {string(".m3u8"),			string("Content-type: application/x-mpegURL\r\n")},
	{string(".ts"),				string("Content-type: video/MP2T\r\n")}
};

static status webser_start( event_t * ev );

static status webser_api_find( string_t * key, event_pt * handler )
{
    uint32 i = 0;
	webser_api_t *s = NULL;

	if( !g_web_ctx->g_api_list ) {
		return ERROR;
	}
	for( i = 0; i < g_web_ctx->g_api_list->elem_num; i ++ ) {
		s = mem_arr_get( g_web_ctx->g_api_list, i + 1 );
		if( s == NULL ) {
			continue;
        }
        
		if( (s->key.len == key->len) && (OK == strncasecmp( (char*)s->key.data, (char*)key->data, key->len ) ) )  {
			if( handler ) {
				*handler = s->handler;
			}
			return OK;
		}
	}
	return ERROR;
}

status webser_api_reg( char * key, event_pt cb )
{
    webser_api_t * p = mem_arr_push( g_web_ctx->g_api_list );
    if( !p ) {
        err("webser g_api_list mem list push failed\n");
		return ERROR;
    }

	p->key.len = strlen(key);
	p->key.data = mem_page_alloc( g_web_ctx->g_api_page, p->key.len+1 );
	if( p->key.data == NULL ) {
		err("page mem alloc failed\n");
		return ERROR;
	}
	memcpy( p->key.data, key, p->key.len );
	p->handler = cb;
	return OK;
}

static char* webser_get_mimetype( unsigned char * str, int len )
{
    uint32 i;

	if( !str || len == 0 ) {
		return (char*)mimetype_table[0].header.data;
	}
	
    for( i =0; i < sizeof(mimetype_table)/sizeof(mime_type_t); i ++ ) {
        if( NULL != l_find_str( str, len, mimetype_table[i].type.data, mimetype_table[i].type.len ) ) {
            return (char*)mimetype_table[i].header.data;
        }
    }
    err("webser [%.*s] mimitype find failed, use default\n", len, str );
    return (char*)mimetype_table[0].header.data;
}

static status webser_alloc( webser_t ** webser )
{
    webser_t * new;
    queue_t * q;

    if( 1 == queue_empty( &g_web_ctx->g_queue_usable ) ) {
        err("webser have't usbale\n");
        return ERROR;
    }
    q = queue_head( &g_web_ctx->g_queue_usable );
    queue_remove( q );
    queue_insert_tail( &g_web_ctx->g_queue_use, q );
    new = ptr_get_struct( q, webser_t, queue );
    *webser = new;
    return OK;
}

static status webser_free( webser_t * webser )
{
    net_free( webser->c );

    webser->data = NULL;
    webser->type = 0;

    /// memory list will be free when connection free
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
	webser->webapi_handler 	= NULL;
	
    if( webser->ffd ) {
        close( webser->ffd );
        webser->ffd = 0;
    }
    webser->fsize = 0;
    webser->fsend = 0;

    webser->http_rsp_mime = NULL;
	webser->http_rsp_bodyn = 0;
	webser->http_rsp_code = 0;
    webser->http_rsp_header_meta = NULL;
    webser->http_rsp_body_meta = NULL;

    queue_remove( &webser->queue );
    queue_insert_tail( &g_web_ctx->g_queue_usable, &webser->queue );
    return OK;
}


inline static void webser_timeout_con( void * data )
{
    net_free( (connection_t*)data );
}

inline static void webser_timeout_cycle( void * data )
{
	webser_free( (webser_t*)data );
}

static status webser_keepalive( event_t * ev )
{
    connection_t* c = ev->data;
    webser_t * webser = c->data;
    uint32 remain = meta_len( c->meta->pos, c->meta->last );

    if( remain ) {
        memcpy( c->meta->start, c->meta->pos, remain );
    }
    c->meta->pos    = c->meta->start;
    c->meta->last   = c->meta->pos + remain;
    
    webser_free( webser );

    ev->write_pt = NULL;
    ev->read_pt = webser_start;
    event_opt( ev, c->fd, EV_R );
    return ev->read_pt( ev );
}

void webser_rsp_mime( webser_t * webser, char * mimetype )
{
    webser->http_rsp_mime = webser_get_mimetype( (unsigned char*)mimetype, l_strlen(mimetype) );
}


/// @brief storge string into memory list 
/// @param p 
/// @param list 
/// @param str 
/// @return 
static int webser_rsp_list_push_str(mem_page_t * p, mem_list_t ** list, char * str )
{
    /// alloc memlist append into the list tail
    mem_list_t * l_new = NULL;

    if( OK != list_page_alloc( p, strlen(str), &l_new ) ) {
        err("webser alloc list form page failed\n");
        return ERROR;
    }
    l_new->datalen = strlen(str);
    memcpy( l_new->data, str, l_new->datalen );

    if( NULL == *list ) {
        *list = l_new;
    } else {
        mem_list_t * tail = *list;
        while( tail->next ) {
            tail = tail->next;
        }
        tail->next = l_new;
    }
    return OK;
}

/// @brief for easy add http rsp payload string 
/// @param webser 
/// @param str 
void webser_rsp_body_push_str( webser_t * webser, char * str )
{
    if( OK != webser_rsp_list_push_str( webser->c->page, &webser->http_rsp_body_list, str ) ) {
        err("webser rsp body list push str [%s] failed\n", str );
    }
	return;	
}

/// @brief for easy add http rsp header string 
/// @param webser 
/// @param str 
void webser_rsp_header_push_str( webser_t * webser, char * str ) 
{
    if( OK != webser_rsp_list_push_str( webser->c->page, &webser->http_rsp_header_list, str ) ) {
        err("webser rsp header list push str [%s] failed\n", str );
    }
    return;
}

static status webser_rsp_send_body_api( event_t * ev )
{	
	connection_t * c = ev->data;
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
    if( webser->http_rsp_code == 200 && webser->http_req->keepalive == 1 ) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    }

    webser_free( webser );
    return OK;
}

static status webser_rsp_send_body_file( event_t * ev )
{	
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;
	
    /// send http payload static file 
    /// (read form file and send unti finish or error happen)
	do {
		rc = c->send_chain( c, webser->http_rsp_body_meta );
        if( rc < 0 ) {
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

        int fread = 0;
	    int fpartn = 0;
		int fremain = webser->fsize - webser->fsend;
		if( fremain <= 0 ) {
			// break the loop when send file success
			break;
		}
		timer_add( &ev->timer, WEBSER_TIMEOUT );
		webser->http_rsp_body_meta->pos = webser->http_rsp_body_meta->last = webser->http_rsp_body_meta->start;
       
        fpartn = (uint32)l_min( fremain, meta_len( webser->http_rsp_body_meta->last, webser->http_rsp_body_meta->end) );
		fread = read( webser->ffd, webser->http_rsp_body_meta->last, fpartn );
        if( fread <= 0 ) {
            err("webser read file, errno [%d]\n", errno );
            webser_free( webser );
            return ERROR;
        }
		webser->http_rsp_body_meta->last += fread;
        webser->fsend += fread;
	} while(1);

	timer_del( &ev->timer );
    if( webser->http_rsp_code == 200 && webser->http_req->keepalive == 1 ) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    } 
    webser_free( webser );
    return OK;
}


static status webser_rsp_send_body( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
  
    /// send http rsp payload
    if( webser->type == WEBSER_FILE ) {
	   
        /// http rsp payload type static file 
        int fremain = webser->fsize - webser->fsend;
        int fread = 0;
        int fpartn = 0;

        fpartn = ( fremain >= WEBSER_BODY_META_LENGTH ? WEBSER_BODY_META_LENGTH : fremain );
        if( OK != meta_alloc_form_mempage( webser->c->page, fpartn, &webser->http_rsp_body_meta ) ) {
            err("webser rsp body meta alloc failed\n");
            webser_free(webser);
            return ERROR;
        }
        fread = read( webser->ffd, webser->http_rsp_body_meta->last, fpartn );
        if( fread <= 0 ) {
            err("webser rsp body read file failed. [%d]\n", errno );
            webser_free(webser);
            return ERROR;
        }
        webser->http_rsp_body_meta->last += fread;
        webser->fsend += fread;
        
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
    connection_t * c = ev->data;
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
		timer_del( &ev->timer );
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

static status webser_try_read( event_t * ev  )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
	int n = 0;
	char buf[1] = {0};

	n = recv(c->fd, buf, 1, MSG_PEEK);
        goto closed;
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

static status webser_rsp_send_start( event_t * ev )
{
    connection_t * c = ev->data;
    ev->read_pt = webser_try_read;
    ev->write_pt = webser_rsp_send_header;
    event_opt( ev, c->fd, EV_R|EV_W );
    return ev->write_pt( ev );
}

static status webser_rsp_header_build( webser_t * webser )
{
	// build http rsp header
    switch( webser->http_rsp_code ) {
        case 200:
			webser_rsp_header_push_str( webser, "HTTP/1.1 200 OK\r\n" );
            break;
        case 400:
			webser_rsp_header_push_str( webser, "HTTP/1.1 400 Bad Request\r\n" );
            break;
        case 404:
			webser_rsp_header_push_str( webser, "HTTP/1.1 404 Not Found\r\n" );
            break;
        case 500:
			webser_rsp_header_push_str( webser, "HTTP/1.1 500 Internal Server Error\r\n" );
            break;
        case 403:
			webser_rsp_header_push_str( webser, "HTTP/1.1 403 Forbidden\r\n" );
            break;
        default:
			webser_rsp_header_push_str( webser, "HTTP/1.1 400 Bad Request\r\n" );
            break;
    }
	webser_rsp_header_push_str( webser, "Server: S5\r\n" );
	webser_rsp_header_push_str( webser, "Accept-Charset: utf-8\r\n" );
    char gmt_date_str[128] = {0};
	sprintf( gmt_date_str, "Date: %s\r\n", systime_gmt());
	webser_rsp_header_push_str( webser, gmt_date_str );
    
	if( !webser->http_rsp_mime ) {
		webser->http_rsp_mime = webser_get_mimetype( NULL, 0 );
	} 
	webser_rsp_header_push_str( webser, webser->http_rsp_mime );
	
    if( webser->http_rsp_bodyn > 0 ) {
        char content_len_str[64] = {0};
        snprintf( content_len_str, sizeof(content_len_str)-1, "Content-Length: %d\r\n", webser->http_rsp_bodyn );
		webser_rsp_header_push_str( webser, content_len_str );
    }
	
    if( webser->http_req->headers.connection && ((uint32)webser->http_req->headers.connection->len > l_strlen("close")) ) {
		webser_rsp_header_push_str( webser, "Connection: keep-alive\r\n" );
    } else {
		webser_rsp_header_push_str( webser, "Connection: close\r\n" );
    }
	webser_rsp_header_push_str( webser, "\r\n" );

    
    /// get http rsp header len  
	mem_list_t * cur = webser->http_rsp_header_list;
	while( cur ) {
		webser->http_rsp_headern += cur->datalen;
		cur = cur->next;
	}
	/// get http rsp header data
	if( webser->http_rsp_headern > 0 ) {
		if( OK != meta_alloc_form_mempage( webser->c->page, webser->http_rsp_headern, &webser->http_rsp_header_meta ) ) {
			err("webser page alloc resp header meta failed\n");
			webser_free( webser );
	        return ERROR;
		}
		cur = webser->http_rsp_header_list;
		while( cur ) {
			memcpy( webser->http_rsp_header_meta->last, cur->data, cur->datalen );
			webser->http_rsp_header_meta->last += cur->datalen;
			cur = cur->next;
		}
	}
    return OK;
}

static void webser_req_file_path( webser_t *webser, char * filepath, int filepathl )
{
    char * p = filepath;
    int32 pn = 0;


    pn += snprintf( p+pn, filepathl-pn, "%s", config_get()->http_home );
    if( webser->http_req->uri.len > 0 ) {
        pn += snprintf( p+pn, filepathl-pn, "%.*s", webser->http_req->uri.len, webser->http_req->uri.data );
        // if url is a dir, need to add index path
        if( webser->http_req->uri.data[webser->http_req->uri.len-1] == '/' ) {
            pn += snprintf( p+pn, filepathl-pn, "%s", config_get()->http_index );
        }
    } else {
        pn += snprintf( p+pn, filepathl-pn, "/%s", config_get()->http_index );
    }
}

static void webser_req_file_open ( webser_t * webser )
{
    struct stat st;
    int code = 200;
    char filepath[WEBSER_LENGTH_PATH_STR] = {0};

    webser_req_file_path( webser, filepath, WEBSER_LENGTH_PATH_STR );
    webser->http_rsp_mime = webser_get_mimetype( (unsigned char*)filepath, strlen(filepath) );

    do {
        if( OK != stat( filepath, &st ) ) {
            err("webser stat check file [%s] failed, [%d]\n", filepath, errno );
            code = 400;
            break;
        }

        switch( st.st_mode & S_IFMT ) {
            case S_IFREG:
                code = ( st.st_mode & S_IRUSR ) ? 200 : 403;
                break;
            default:
                code = 404;
                break;
        }

        if( code == 200 ) {
            webser->ffd = open( filepath, O_RDONLY );
            if( -1 == webser->ffd ) {
                err("webser open static file failed [%s], errno [%d]\n", filepath, errno );
                code = 500;
                break;
            }
        }
    } while(0);
    webser->fsize = ( code == 200 ) ? st.st_size : 0;
    webser->fsend = 0;

	webser->http_rsp_bodyn = webser->fsize;
	webser->http_rsp_code = code;
    return;
}

static status webser_req_file ( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;

    webser_req_file_open( webser );
	
    if( OK != webser_rsp_header_build( webser ) ) {
        err("webser resp head build failed\n");
        webser_free( webser );
        return ERROR;
    }
	
    ev->read_pt = webser_rsp_send_start;
    return ev->read_pt( ev );
}

status webser_req_api( event_t * ev )
{
    int32 resp_code = 0;
    connection_t * c = ev->data;
    webser_t * webser = c->data;

	/// goto do resiger api function 
    resp_code = webser->webapi_handler( ev );
	
    webser->http_rsp_code = resp_code;
	webser->http_rsp_bodyn = 0;

	/// get http rsp payload len
	mem_list_t * cur = webser->http_rsp_body_list;
	while( cur ) {
		webser->http_rsp_bodyn += cur->datalen;
		cur = cur->next;
	}

	/// get http rsp payload data
	if( webser->http_rsp_bodyn > 0 ) {
		if( OK != meta_alloc_form_mempage( c->page, webser->http_rsp_bodyn, &webser->http_rsp_body_meta ) ) {
			err("webser page alloc resp body meta failed\n");
			webser_free( webser );
	        return ERROR;
		}
		cur = webser->http_rsp_body_list;
		while( cur ) {
			memcpy( webser->http_rsp_body_meta->last, cur->data, cur->datalen );
			webser->http_rsp_body_meta->last += cur->datalen;
			cur = cur->next;
		}
	}

	/// build http rsp header
    if( OK != webser_rsp_header_build( webser ) ) {
        err("webser response head build failed\n");
        webser_free( webser );
        return ERROR;
    }
    ev->read_pt = webser_rsp_send_start;
    return ev->read_pt( ev );
}

static status webser_req_bypass( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

	/// static file request or api request  
	rc = webser_api_find( &webser->http_req->uri, &webser->webapi_handler );
    if( rc == OK ) {
        webser->type = WEBSER_API;
        ev->read_pt = webser_req_api;
    } else {
        webser->type = WEBSER_FILE;
        ev->read_pt	= webser_req_file;
    }
    return ev->read_pt( ev );
}

static status webser_req_body( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    /// goto parse http body, content-length or chunk
    rc = webser->http_req_body->cb( webser->http_req_body );
    if( rc < 0 ) {
        if( rc == ERROR ) {
            err("webser process body failed\n");
            webser_free(webser);
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt( &ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }

    timer_del( &ev->timer );
    ev->read_pt	= webser_req_bypass;
    return ev->read_pt( ev );	
}

static status webser_req_header( event_t * ev )
{
    connection_t * c = ev->data;
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

	/// if http request have body, then goto receive it 
	if( http_req_have_body( webser->http_req ) ) {
		if( OK != http_body_create( webser->c, &webser->http_req_body, 0 ) ) {
            err("http_body_create failed\n");
            webser_free( webser );
            return ERROR;
        }
		webser->http_req_body->body_type = webser->http_req->content_type;
        webser->http_req_body->content_len = webser->http_req->content_len;
		ev->read_pt	= webser_req_body;
		return ev->read_pt( ev );
	}

	/// if don't have body, excute down 
	ev->read_pt	= webser_req_bypass;
    return ev->read_pt( ev );
}

static status webser_start( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = NULL;

    // init web connection page and meta memory for use
    if( NULL == c->page ) {
        if( OK != mem_page_create(&c->page, L_PAGE_DEFAULT_SIZE) ) {
            err("webser con page alloc failed\n");
            net_free( c );
            return ERROR;
        }
    }
    if( NULL == c->meta ) {
        if( OK != meta_alloc_form_mempage( c->page, WEBSER_REQ_META_LEN, &c->meta ) ) {
            err("webser con meta alloc failed\n");
            net_free( c );
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
    ev->read_pt = webser_req_header;
    return ev->read_pt( ev );
}

/// @brief  transfer request to s5 module 
/// @param ev 
/// @return 
static status webser_transfer_to_s5( event_t * ev )
{
    connection_t * c = ev->data;
    meta_t * meta = NULL;
    ssize_t rc = 0;
    
    // page && meta init
    if( NULL == c->page ) {
        if( OK != mem_page_create(&c->page, L_PAGE_DEFAULT_SIZE) ) {
            err("webser c page create failed\n");
            net_free( c );
            return ERROR;
        }
    }
    if( NULL == c->meta ) {
        if( OK != meta_alloc_form_mempage( c->page, WEBSER_REQ_META_LEN, &c->meta ) ) {
            err("webser alloc con meta failed\n");
            net_free( c );
            return ERROR;
        }
    }

	/// try to recv s5 private authorization header 
    meta = c->meta;
    while( meta_len( meta->pos, meta->last ) < sizeof(s5_auth_info_t) ) {
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
        s5_auth_info_t * header = (s5_auth_info_t*)meta->pos;
        
        if( S5_AUTH_MAGIC_NUM != ntohl( header->magic ) ) {
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
    connection_t * c = ev->data;

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
    connection_t * c = ev->data;
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
    uint32 i = 0;
    int ret = -1;

	if( g_web_ctx ) {
		err("g_web_ctx not empty\n");
        return ERROR;
	}

    do {
        g_web_ctx = l_safe_malloc(sizeof(g_web_t)+ (MAX_NET_CON*sizeof(webser_t)) );
        if( !g_web_ctx ) {
            err("webser alloc this failed, [%d]\n", errno );
            return ERROR;
        }
		
        queue_init( &g_web_ctx->g_queue_use );
        queue_init( &g_web_ctx->g_queue_usable );
        for( i = 0; i < MAX_NET_CON; i ++ ) {
			queue_insert_tail( &g_web_ctx->g_queue_usable, &g_web_ctx->g_pool[i].queue );
		}
        
        /// init webserv api data
        if( OK == mem_page_create( &g_web_ctx->g_api_page, L_PAGE_DEFAULT_SIZE ) )  {
			if( OK == mem_arr_create( &g_web_ctx->g_api_list, sizeof(webser_api_t) ) ) {
				webapi_init();
				ret = 0;
			} else {
				err("webser create api mem arr create failed\n" );
            	break;
			}
		} else {
			err("webser api mem page create failed\n");
            break;	
		}
    } while(0);

    if( 0 != ret ) {
        if( g_web_ctx ) {
            if( g_web_ctx->g_api_list ) {
                mem_arr_free( g_web_ctx->g_api_list );
                g_web_ctx->g_api_list = NULL;
            }
            
            if( g_web_ctx->g_api_page ) {
                mem_page_free( g_web_ctx->g_api_page );
                g_web_ctx->g_api_page = NULL;
            }

            l_safe_free(g_web_ctx);
            g_web_ctx = 0;
        }
    }
    return (0 == ret)? OK:ERROR;
}

status webser_end( void )
{
    if( g_web_ctx ) {
        if( g_web_ctx->g_api_list ) {
                mem_arr_free( g_web_ctx->g_api_list );
                g_web_ctx->g_api_list = NULL;
            }
            
            if( g_web_ctx->g_api_page ) {
                mem_page_free( g_web_ctx->g_api_page );
                g_web_ctx->g_api_page = NULL;
            }
            l_safe_free( g_web_ctx );
    }
    return OK;
}

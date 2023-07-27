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

	p->key.len     = strlen(key);
	p->key.data    = mem_page_alloc( g_web_ctx->g_api_page, p->key.len+1 );
	if( p->key.data == NULL ) {
		err("page mem alloc failed\n");
		return ERROR;
	}
	memcpy( p->key.data, key, p->key.len );
	p->handler = cb;
	return OK;
}

static string_t * webser_get_mimetype( unsigned char * str, int len )
{
    uint32 i;

	if( !str || len == 0 )
	{
		return &mimetype_table[0].header;
	}
	
    for( i =0; i < sizeof(mimetype_table)/sizeof(mime_type_t); i ++ )
    {
        if( NULL != l_find_str( str, len, mimetype_table[i].type.data, mimetype_table[i].type.len ) )
        {
            return &mimetype_table[i].header;
        }
    }
    err("webser [%.*s] mimitype find failed, use default\n", len, str );
    return &mimetype_table[0].header;
}


void webapi_resp_string( webser_t * webser, char * str )
{
    mem_list_t * last = NULL;
	mem_list_t * new = NULL;

	if( OK != list_page_alloc( webser->c->page, l_strlen(str), &new ) )
	{
		err("webser page alloc list failed\n");
		return;
	}
	new->datalen = l_strlen( str );
	memcpy( new->data, str, new->datalen );

	if( webser->http_resp_body_list )
	{
		last = webser->http_resp_body_list;
		while( last->next )
		{
			last = last->next;
		}
		last->next = new;
	}
	else
	{
		webser->http_resp_body_list = new;
	}
	return;	
}

void webapi_resp_mimetype( webser_t * webser, char * mimetype )
{
    webser->http_resp_mime = webser_get_mimetype( (unsigned char*)mimetype, l_strlen(mimetype) );
}

static status webser_alloc( webser_t ** webser )
{
    webser_t * new;
    queue_t * q;

    if( 1 == queue_empty( &g_web_ctx->g_queue_usable ) )
    {
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
    webser->data 			= NULL;
    webser->type 			= 0;

	webser->http_resp_body_list = NULL;
    if( webser->http_resp_head_list )
    {
		mem_arr_free( webser->http_resp_head_list );
		webser->http_resp_head_list = NULL;
	}
    if( webser->http_req )
    {
        http_req_free( webser->http_req );
		webser->http_req = NULL;
    }
    if( webser->http_req_body )
    {
        http_body_free( webser->http_req_body );
		webser->http_req_body = NULL;
    }
	webser->webapi_handler 	= NULL;
	
    if( webser->filefd )
    {
        close( webser->filefd );
        webser->filefd = 0;
    }
    webser->filelen			= 0;
    webser->filesend		= 0;

    webser->http_resp_mime  	= NULL;
	webser->http_resp_bodylen 	= 0;
	webser->http_resp_code 		= 0;
    webser->http_resp_head   = NULL;
    webser->http_resp_body   = NULL;

    queue_remove( &webser->queue );
    queue_insert_tail( &g_web_ctx->g_queue_usable, &webser->queue );
    return OK;
}

static status webser_over( webser_t * webser )
{
    net_free( webser->c );
    webser_free( webser );
    return OK;
}

inline static void webser_timeout_con( void * data )
{
    net_free( (connection_t*)data );
}

inline static void webser_timeout_cycle( void * data )
{
	webser_over( (webser_t*)data );
}

static status webser_keepalive( event_t * ev )
{
    connection_t* c = ev->data;
    webser_t * webser = c->data;
    uint32 remain = meta_len( c->meta->pos, c->meta->last );

    if( remain )
    {
        memcpy( c->meta->start, c->meta->pos, remain );
    }
    c->meta->pos    = c->meta->start;
    c->meta->last   = c->meta->pos + remain;
    
    webser_free( webser );

    ev->write_pt 	= NULL;
    ev->read_pt 	= webser_start;
    event_opt( ev, c->fd, EV_R );
    return ev->read_pt( ev );
}


static status webser_resp_send_body_str( event_t * ev )
{	
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;
	
	if( webser->http_resp_body )
	{
		rc = c->send_chain( c, webser->http_resp_body );
	    if( rc < 0 )
	    {
	        if( rc == ERROR )
	        {
	            err("webser send resp body failed\n");
	            webser_over( webser );
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, webser );
	        timer_set_pt( &ev->timer, webser_timeout_cycle );
	        timer_add( &ev->timer, WEBSER_TIMEOUT );
	        return AGAIN;
	    }
	}

	timer_del( &ev->timer );
    if( webser->http_resp_code == 200 && webser->http_req->keepalive == 1 )
    {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    }
    return webser_over( webser );
}

static status webser_resp_send_body_file( event_t * ev )
{	
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;
	uint32 len = 0;

	do
	{
		rc = c->send_chain( c, webser->http_resp_body );
        if( rc < 0 )
        {
            if( rc == ERROR )
            {
                err("webser send resp body failed\n");
                webser_over( webser );
                return ERROR;
            }
            timer_set_data( &ev->timer, webser );
            timer_set_pt( &ev->timer, webser_timeout_cycle );
            timer_add( &ev->timer, WEBSER_TIMEOUT );
            return AGAIN;
        }

	
		ssize_t size = 0, unfinish = webser->filelen - webser->filesend;
		if( unfinish <= 0 )
		{
			// break the loop when send file success
			debug("webser send body success\n");
			break;
		}
		timer_add( &ev->timer, WEBSER_TIMEOUT );
	
		webser->http_resp_body->pos = webser->http_resp_body->last = webser->http_resp_body->start;
		len = (uint32)l_min( unfinish, meta_len( webser->http_resp_body->last, webser->http_resp_body->end) );

		size = read( webser->filefd, webser->http_resp_body->last, len );
        if( size <= 0 )
        {
            err("webser read file, errno [%d]\n", errno );
            webser_over( webser );
            return ERROR;
        }
		webser->http_resp_body->last 	+= size;
        webser->filesend            	+= size;
	} while(1);

	timer_del( &ev->timer );
    if( webser->http_resp_code == 200 && webser->http_req->keepalive == 1 )
    {
        ev->write_pt = webser_keepalive;
        return ev->write_pt( ev );
    }
    return webser_over( webser );
}


static status webser_resp_send_body( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
  
    if( webser->type == WEBSER_FILE )
    {
		uint32 len = 0;
	    ssize_t rc = 0;
	    ssize_t unfinish = webser->filelen - webser->filesend;

	    len = (uint32)l_min( unfinish, WEBSER_BODY_META_LENGTH );
	    if( OK != meta_alloc_form_mempage( webser->c->page, len, &webser->http_resp_body ) )
	    {
	        err("webser resp body meta alloc failed\n");
	        return ERROR;
	    }
	    rc = read( webser->filefd, webser->http_resp_body->last, len );
	    if( rc <= 0 )
	    {
	        err("webser read request file, errno [%d]\n", errno );
	        return ERROR;
	    }
	    webser->http_resp_body->last     	+= rc;
	    webser->filesend                	+= rc;

		ev->write_pt = webser_resp_send_body_file;
		return ev->write_pt( ev );
	}
	else
	{
		ev->write_pt = webser_resp_send_body_str;
		return ev->write_pt( ev );
	}
}

static status webser_resp_send_head( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    rc = c->send_chain( c, webser->http_resp_head );
    if( rc < 0 )
    {
        if( rc == ERROR )
        {
            err("webser resp head send failed\n");
            webser_over( webser );
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt( &ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

	if( webser->http_resp_bodylen <= 0 )
	{
		timer_del( &ev->timer );
	    if( webser->http_resp_code == 200 && webser->http_req->keepalive == 1 )
	    {
	        ev->write_pt = webser_keepalive;
	        return ev->write_pt( ev );
	    }
	    return webser_over( webser );	
	}

    ev->write_pt	= webser_resp_send_body;
    return ev->write_pt( ev );
}

static status webser_try_read( event_t * ev  )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
	int n = 0;
	char buf[1] = {0};

	n = recv(c->fd, buf, 1, MSG_PEEK);
    if (n == 0) 
	{
        goto closed;
    } 
	else if (n == -1)
	{
        if (errno != AGAIN) 
		{
            goto closed;
        }
    }

    return OK;
closed:
	err("webser try read failed\n");
    webser_over( webser );
    
    return ERROR;
}

status webser_resp_send( event_t * ev )
{
    connection_t * c = ev->data;
    ev->read_pt   = webser_try_read;
    ev->write_pt  = webser_resp_send_head;
    event_opt( ev, c->fd, EV_R|EV_W );
    return ev->write_pt( ev );
}

static void webser_resp_head_push( mem_arr_t * list, char * str )
{
	webser_resp_head_line_t * new = mem_arr_push( list );
	if( new )
	{
		memcpy( new->str, str, strlen(str) );
	}
}

status webser_resp_head( webser_t * webser )
{
    char content_len_str[64] = {0};
    uint32 head_len = 0, cur_len = 0;
	int i = 0;

	if( OK != mem_arr_create( &webser->http_resp_head_list, sizeof(webser_resp_head_line_t) ) )
	{
		err("webser alloc http resp head list");
		return ERROR;
	}

	// build resp head 
    switch( webser->http_resp_code )
    {
        case 200:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 200 OK\r\n" );
            break;
        case 400:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 400 Bad Request\r\n" );
            break;
        case 404:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 404 Not Found\r\n" );
            break;
        case 500:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 500 Internal Server Error\r\n" );
            break;
        case 403:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 403 Forbidden\r\n" );
            break;
        default:
			webser_resp_head_push( webser->http_resp_head_list, "HTTP/1.1 400 Bad Request\r\n" );
            break;
    }


    if( webser->http_resp_bodylen > 0 )
    {
    	memset( content_len_str, 0, sizeof(content_len_str) );
        snprintf( content_len_str, sizeof(content_len_str)-1, "Content-Length: %d\r\n", (uint32)webser->http_resp_bodylen );
		webser_resp_head_push( webser->http_resp_head_list, content_len_str );
		if( webser->http_resp_mime->data == NULL )
		{
			webser->http_resp_mime = webser_get_mimetype( NULL, 0 );
		}
		else
		{
			webser_resp_head_push( webser->http_resp_head_list, (char*)webser->http_resp_mime->data );
		}
    }
	webser_resp_head_push( webser->http_resp_head_list, "Server: lkwebv1\r\n" );
	webser_resp_head_push( webser->http_resp_head_list, "Accept-Charset: utf-8\r\n" );
	char serv_gmt_date[128] = {0};
	sprintf( serv_gmt_date, "Date: %s\r\n", systime_gmt());
	webser_resp_head_push( webser->http_resp_head_list, serv_gmt_date );
	
    if( webser->http_req->headers.connection && ((uint32)webser->http_req->headers.connection->len > l_strlen("close")) )
    {
		webser_resp_head_push( webser->http_resp_head_list, "Connection: keep-alive\r\n" );
    }
    else
    {
		webser_resp_head_push( webser->http_resp_head_list, "Connection: close\r\n" );
    }
	webser_resp_head_push( webser->http_resp_head_list, "\r\n" );


	head_len = 0;
	webser_resp_head_line_t * head_line;

	for( i = 0; i < webser->http_resp_head_list->elem_num; i ++ ) 
	{
		head_line = mem_arr_get( webser->http_resp_head_list, i+1 );
		head_len += strlen(head_line->str);
	}
    if( OK != meta_alloc_form_mempage( webser->c->page, head_len+1, &webser->http_resp_head ) )
    {
        err("webser build resp head all meta failed\n");
        return ERROR;
    }
	for( i = 0; i < webser->http_resp_head_list->elem_num; i ++ ) 
	{
		head_line = mem_arr_get( webser->http_resp_head_list, i+1 );
		memcpy( (char*)webser->http_resp_head->pos + cur_len, head_line->str, strlen(head_line->str) );
		cur_len += strlen(head_line->str);
	}
    webser->http_resp_head->last += head_len;
    return OK;
}

static void webser_file_path( webser_t *webser, char * path, int str_len )
{
    int32 len = 0;
    char * ptr = path;

    len += snprintf( ptr+len, str_len-len, "%s", config_get()->http_home );
    if( webser->http_req->uri.len > 0 )
    {
        len += snprintf( ptr+len, str_len-len, "%.*s", webser->http_req->uri.len, webser->http_req->uri.data );
        // if url is a dir, need to add index path
        if( webser->http_req->uri.data[webser->http_req->uri.len-1] == '/' )
        {
            len += snprintf( ptr+len, str_len-len, "%s", config_get()->http_index );
        }
    }
    else
        len += snprintf( ptr+len, str_len-len, "/%s", config_get()->http_index );
}

static void webser_file_check ( webser_t * webser )
{
    struct stat st;
    int code = 200;
    char local_path[WEBSER_LENGTH_PATH_STR] = {0};

    webser_file_path( webser, local_path, WEBSER_LENGTH_PATH_STR );
    webser->http_resp_mime = webser_get_mimetype( (unsigned char*)local_path, l_strlen(local_path) );

    do
    {
        if( OK != stat( local_path, &st ) )
        {
            err("webser stat check file [%s] failed, [%d]\n", local_path, errno );
            code = 500;
            break;
        }

        switch( st.st_mode & S_IFMT )
        {
            case S_IFREG:
                code = ( st.st_mode & S_IRUSR ) ? 200 : 403;
                break;
            default:
                code = 404;
                break;
        }

        if( code == 200 )
        {
            webser->filefd = open( local_path, O_RDONLY );
            if( -1 == webser->filefd )
            {
                err("webser open static file failed [%s], errno [%d]\n", local_path, errno );
                code = 500;
                break;
            }
        }
    } while(0);
    webser->filelen         = ( code == 200 ) ? st.st_size : 0;
    webser->filesend        = 0;


	webser->http_resp_bodylen	= webser->filelen;
	webser->http_resp_code  	= code;
    return;
}

static status webser_file ( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;

    webser_file_check( webser );
	
    if( OK != webser_resp_head( webser ) )
    {
        err("webser resp head build failed\n");
        webser_over( webser );
        return ERROR;
    }
	
    ev->read_pt = webser_resp_send;
    return ev->read_pt( ev );
}

status webser_api( event_t * ev )
{
    int32 resp_code = 0;
    connection_t * c = ev->data;
    webser_t * webser = c->data;
	
    resp_code= webser->webapi_handler( ev );
    webser->http_resp_code = resp_code;

	// build api resp body
	webser->http_resp_bodylen = 0;
	mem_list_t * cur = webser->http_resp_body_list;
	while( cur )
	{
		webser->http_resp_bodylen += cur->datalen;
		cur = cur->next;
	}
	if( webser->http_resp_bodylen > 0 )
	{
		if( OK != meta_alloc_form_mempage( c->page, webser->http_resp_bodylen, &webser->http_resp_body ) )
		{
			err("webser page alloc resp body meta failed\n");
			webser_over( webser );
	        return ERROR;
		}
		cur = webser->http_resp_body_list;
		while( cur )
		{
			memcpy( webser->http_resp_body->last, cur->data, cur->datalen );
			webser->http_resp_body->last += cur->datalen;
			cur = cur->next;
		}
	}
	
    if( OK != webser_resp_head( webser ) )
    {
        err("webser response head build failed\n");
        webser_over( webser );
        return ERROR;
    }
    ev->read_pt = webser_resp_send;
    return ev->read_pt( ev );
}

static status webser_switch( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;
	
	rc = webser_api_find( &webser->http_req->uri, &webser->webapi_handler );
    if( rc == OK ) {
        webser->type 	= WEBSER_API;
        ev->read_pt 	= webser_api;
    } else {
        webser->type 	= WEBSER_FILE;
        ev->read_pt		= webser_file;
    }
    return ev->read_pt( ev );
}

static status webser_process_http_body( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    rc = webser->http_req_body->cb( webser->http_req_body );
    if( rc < 0 )
    {
        if( rc == ERROR )
        {
            err("webser process body failed\n");
            webser_over(webser);
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt( &ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }

    timer_del( &ev->timer );
    ev->read_pt	=	webser_switch;
    return ev->read_pt( ev );	
}

static status webser_process_http_req( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
    status rc = 0;

    rc = webser->http_req->cb( webser->http_req );
    if( rc < 0 )
    {
        if( rc == ERROR )
        {
            err("webser process request header failed\n");
            webser_over( webser );
            return ERROR;
        }
        timer_set_data( &ev->timer, webser );
        timer_set_pt(&ev->timer, webser_timeout_cycle );
        timer_add( &ev->timer, WEBSER_TIMEOUT );
        return AGAIN;
    }
    timer_del( &ev->timer );

	if( http_req_have_body( webser->http_req ) )
	{
		if( OK != http_body_create( webser->c, &webser->http_req_body, 0 ) )
        {
            err("http_body_create failed\n");
            webser_over( webser );
            return ERROR;
        }
		webser->http_req_body->body_type 		= webser->http_req->content_type;
        webser->http_req_body->content_length 	= webser->http_req->content_length;
		ev->read_pt	=	webser_process_http_body;
		return ev->read_pt( ev );
	}
	
	ev->read_pt	=	webser_switch;
    return ev->read_pt( ev );
}

static status webser_start( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = NULL;

    // init web connection page and meta memory for use
    if( NULL == c->page )
    {
        if( OK != mem_page_create(&c->page, L_PAGE_DEFAULT_SIZE) )
        {
            err("webser con page alloc failed\n");
            net_free( c );
            return ERROR;
        }
    }
    if( NULL == c->meta )
    {
        if( OK != meta_alloc_form_mempage( c->page, WEBSER_REQ_META_LEN, &c->meta ) )
        {
            err("webser con meta alloc failed\n");
            net_free( c );
            return ERROR;
        }
    }
    
    if( OK != webser_alloc( &webser ) )
    {
        err("webser con webser alloc failed\n");
        net_free(c);
        return ERROR;
    }
    webser->c   = c;
    c->data     = webser;

    if( OK != http_req_create( c, &webser->http_req ) )
    {
        err("webser alloc req failed\n");
        webser_over( webser );
        return ERROR;
    }
    ev->write_pt 	= NULL;
    ev->read_pt  	= webser_process_http_req;
    return ev->read_pt( ev );
}

static status webser_convert_to_s5( event_t * ev )
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
    
    do {
        s5_auth_info_t * header = (s5_auth_info_t*)meta->pos;
        
        if( S5_AUTH_MAGIC_NUM != header->magic ) {
            // magic number not match, goto http/https process
            break;
        }
        // magic number match success, goto s5 process
        ev->read_pt = socks5_server_secret_start;
        return ev->read_pt( ev );
    } while(0);
    
    ev->read_pt = webser_start;
    return ev->read_pt( ev );
}

static status webser_accept_cb_ssl_check( event_t * ev )
{
    connection_t * c = ev->data;

    if( !c->ssl->handshaked )
    {
        err("webser handshake failed\n");
        net_free( c );
        return ERROR;
    }
    timer_del( &c->event->timer );
    c->recv 			= ssl_read;
    c->send 			= ssl_write;
    c->recv_chain 		= NULL;
    c->send_chain 		= ssl_write_chain;

    ev->write_pt 	    = NULL;
    ev->read_pt  	    = ( config_get()->s5_mode == SOCKS5_SERVER_SECRET) ? webser_convert_to_s5 : webser_start;
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



status webser_init( void )
{
    uint32 i = 0;
    int ret = -1;

    do {
        if( g_web_ctx ) {
            err("g_web_ctx not empty\n");
            return ERROR;
        }
        g_web_ctx = l_safe_malloc(sizeof(g_web_t)+MAX_NET_CON*sizeof(webser_t));
        if( !g_web_ctx ) {
            err("webser alloc this failed, [%d]\n", errno );
            return ERROR;
        }
        memset(g_web_ctx, 0, sizeof(g_web_t)+MAX_NET_CON*sizeof(webser_t));
        queue_init( &g_web_ctx->g_queue_use );
        queue_init( &g_web_ctx->g_queue_usable );
        for( i = 0; i < MAX_NET_CON; i ++ )
            queue_insert_tail( &g_web_ctx->g_queue_usable, &g_web_ctx->g_pool[i].queue );
        
        /// init webserv api data
        if( OK != mem_arr_create( &g_web_ctx->g_api_list, sizeof(webser_api_t) ) ) {
            err("webser create api mem arr create failed\n" );
            break;
        }
        if( OK != mem_page_create( &g_web_ctx->g_api_page, L_PAGE_DEFAULT_SIZE ) ){
            err("webser api mem page create failed\n");
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

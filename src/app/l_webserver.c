#include "l_base.h"
#include "l_http_body.h"
#include "l_http_request_head.h"
#include "l_http_response_head.h"
#include "l_webserver.h"
#include "l_webapi.h"

typedef struct private_webser
{
    queue_t     g_queue_use;
    queue_t     g_queue_usable;
    webser_t    g_pool[0];
} private_webser_t;
static private_webser_t * this = NULL;

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
	{string(".mp3"),			string("Content-type: audio/mpeg\r\n")}
};

static status webser_cycle_init( event_t * ev );

static string_t * webser_get_mimetype( unsigned char * str, int len )
{
    uint32 i;
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

static status webser_alloc( webser_t ** webser )
{
	webser_t * new;
	queue_t * q;

	if( 1 == queue_empty( &this->g_queue_usable ) )
	{
		err("webser have't usbale\n");
		return ERROR;
	}
	q = queue_head( &this->g_queue_usable );
	queue_remove( q );
	queue_insert_tail( &this->g_queue_use, q );
	new = l_get_struct( q, webser_t, queue );
	*webser = new;
	return OK;
}

static status webser_free( webser_t * webser )
{
	meta_t * meta, *next;
	
	webser->type 			= 0;
	webser->data 			= NULL;
	webser->http_resp_code 	= 0;
	webser->webapi_handler 	= NULL;
	if( webser->http_req_head ) 
	{
		webser->http_req_head->handler.exit( webser->http_req_head );
		webser->http_req_head = NULL;
	}	
	if( webser->http_resp_body ) 
	{
		webser->http_resp_body->handler.exit( webser->http_resp_body );
		webser->http_resp_body = NULL;
	}
	
	if( webser->filefd ) 
	{
		close( webser->filefd );
		webser->filefd = 0;
	}
	webser->file_mime  		= NULL;
	webser->filelen			= 0;
	webser->filesend		= 0;
	
	meta = webser->response_head;
	while( meta )
	{
		next = meta->next;
		meta_free( meta );
		meta = next;
	}
	meta = webser->response_body;
	while( meta )
	{
		next = meta->next;
		meta_free( meta );
		meta = next;
	}

	queue_remove( &webser->queue );
	queue_insert_tail( &this->g_queue_usable, &webser->queue );
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
        debug("keep alive remain [%.*s]\n", remain, c->meta->pos );
        memcpy( c->meta->start, c->meta->pos, remain );
        c->meta->pos    = c->meta->start;
        c->meta->last   = c->meta->pos + remain;
    }
    
	webser_free( webser );

	ev->write_pt 	= NULL;
	ev->read_pt 	= webser_cycle_init;
	event_opt( ev, c->fd, EV_R );
	return ev->read_pt( ev );
}


static status webser_resp_send_body( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	status rc = 0;
	uint32 len = 0;

	do
	{
		rc = c->send_chain( c, webser->response_body );
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

		if( webser->type == WEBSER_STATIC )
		{
			ssize_t size = 0, unfinish = webser->filelen - webser->filesend;
			if( unfinish <= 0 )break;

			timer_add( &ev->timer, WEBSER_TIMEOUT );
			webser->response_body->pos = webser->response_body->last = webser->response_body->start;
			len = (uint32)l_min( unfinish, meta_len( webser->response_body->last, webser->response_body->end) );
			
			size = read( webser->filefd, webser->response_body->last, len );
			if( size <= 0 )
			{
				err("webser read request file, errno [%d]\n", errno );
				webser_over( webser );
				return ERROR;
			}
			webser->response_body->last += size;
            webser->filesend            += size;
            continue;
		}
	}
	while (0);
	
	timer_del( &ev->timer );
	if( webser->http_req_head->keepalive == 1 )
	{
		ev->write_pt = webser_keepalive;
		return ev->write_pt( ev );
	}
	return webser_over( webser );
}

static status webser_resp_send_head( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	status rc = 0;

	rc = c->send_chain( c, webser->response_head );
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
	
	ev->write_pt	= webser_resp_send_body;
	return ev->write_pt( ev );
}

static status webser_request_try_read( event_t * ev  )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	if( OK != l_socket_check_status( c->fd ) )
	{
		err("webser check con fd status error\n");
		webser_over( webser );
		return ERROR;
	}
	return OK;
}

status webser_response( event_t * ev )
{
    connection_t * c = ev->data;
    ev->read_pt   = webser_request_try_read;
	ev->write_pt  = webser_resp_send_head;
	event_opt( ev, c->fd, EV_R|EV_W );
	return ev->write_pt( ev );
}

static status webser_process_resp_body_static_file_build( webser_t * webser )
{
	uint32 len = 0;
	ssize_t rc = 0;
	ssize_t unfinish = webser->filelen - webser->filesend;

	len = (uint32)l_min( unfinish, WEBSER_BODY_META_LENGTH );
	if( OK != meta_alloc( &webser->response_body, len ) ) 
	{
		err("webser resp body meta alloc\n");
		return ERROR;
	}
	rc = read( webser->filefd, webser->response_body->last, len );
	if( rc <= 0 )
	{
		err("webser read request file, errno [%d]\n", errno );
		return ERROR;
	}
	webser->response_body->last     += rc;
    webser->filesend                += rc;
	return OK;
}

status webser_process_resp_head_build( webser_t * webser )
{
	char content_len_str[1024] = {0};
	uint32 head_len = 0, cur_len = 0;
	char * ptr = NULL;

	switch( webser->http_resp_code )
	{
		case 200:
			head_len += l_strlen("HTTP/1.1 200 OK\r\n");
			break;
		case 400:
			head_len += l_strlen("HTTP/1.1 400 Bad Request\r\n");
			break;
		case 404:
			head_len += l_strlen("HTTP/1.1 404 Not Found\r\n");
			break;
		case 500:
			head_len += l_strlen("HTTP/1.1 500 Internal Server Error\r\n");
			break;
		case 403:
			head_len += l_strlen("HTTP/1.1 403 Forbidden\r\n");
			break;
		default:
			head_len += l_strlen("HTTP/1.1 400 Bad Request\r\n");
			break;
	}

	if( webser->filelen > 0 )
	{
		head_len += snprintf( content_len_str, sizeof(content_len_str), "Content-Length: %d\r\n", (uint32)webser->filelen );
		head_len += webser->file_mime->len;
	}
	head_len += l_strlen("Server: LKweb_V1.0\r\n");
	head_len += l_strlen("Accept-Charset: utf-8\r\n");

	if( webser->http_req_head->headers_in.connection && ((uint32)webser->http_req_head->headers_in.connection->len > l_strlen("close")) )
	{
		head_len += l_strlen("Connection: keep-alive\r\n");
	} 
	else 
	{
		head_len += l_strlen("Connection: close\r\n");
	} 
	head_len += l_strlen("\r\n");


	if( OK != meta_alloc( &webser->response_head, head_len+1 ) ) 
	{
		err("webser build resp head all meta failed\n");
		return ERROR;
	}
	
	ptr = (char*)webser->response_head->start;
	switch( webser->http_resp_code )
	{
		case 200:
			strcpy( ptr+cur_len, "HTTP/1.1 200 OK\r\n" );
			cur_len += l_strlen("HTTP/1.1 200 OK\r\n");
			break;
		case 400:
			strcpy( ptr+cur_len, "HTTP/1.1 400 Bad Request\r\n" );
			cur_len += l_strlen("HTTP/1.1 400 Bad Request\r\n");
			break;
		case 404:
			strcpy( ptr+cur_len, "HTTP/1.1 404 Not Found\r\n" );
			cur_len += l_strlen("HTTP/1.1 404 Not Found\r\n");
			break;
		case 500:
			strcpy( ptr+cur_len, "HTTP/1.1 500 Internal Server Error\r\n" );
			cur_len += l_strlen("HTTP/1.1 500 Internal Server Error\r\n");
			break;
		case 403:
			strcpy( ptr+cur_len, "HTTP/1.1 403 Forbidden\r\n" );
			cur_len += l_strlen("HTTP/1.1 403 Forbidden\r\n");
			break;
		default:
			strcpy( ptr+cur_len, "HTTP/1.1 400 Bad Request\r\n" );
			cur_len += l_strlen("HTTP/1.1 400 Bad Request\r\n");
			break;
	}
	
	if( webser->filelen > 0 )
	{
		strcpy( ptr+cur_len, content_len_str );
		cur_len += l_strlen(content_len_str);

		memcpy( ptr+cur_len, webser->file_mime->data, webser->file_mime->len );
		cur_len += webser->file_mime->len;
	}
	strcpy( ptr+cur_len, "Server: LKweb_V1.0\r\n" );
	cur_len += l_strlen("Server: LKweb_V1.0\r\n");

	strcpy( ptr+cur_len, "Accept-Charset: utf-8\r\n" );
	cur_len += l_strlen("Accept-Charset: utf-8\r\n");
	
	if( webser->http_req_head->headers_in.connection && ((uint32)webser->http_req_head->headers_in.connection->len > l_strlen("close")) )
	{
		strcpy( ptr+cur_len, "Connection: keep-alive\r\n" );
		cur_len += l_strlen("Connection: keep-alive\r\n");
	}
	else
	{
		strcpy( ptr+cur_len, "Connection: close\r\n" );
		cur_len += l_strlen("Connection: close\r\n");
	}
	strcpy( ptr+cur_len, "\r\n" );
	cur_len += l_strlen("\r\n");

	if( cur_len != head_len )
	{
		err("webser header build failed, curlen [%d] head_len [%d]\n", cur_len, head_len );
		return ERROR;
	}
	webser->response_head->last += head_len;
	return OK;
}

status webser_process_req_body( event_t * ev )
{
	status rc;
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	rc = webser->http_resp_body->handler.process( webser->http_resp_body );
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
	return webser->http_resp_body->callback( ev );
}

static void webser_process_req_static_file_path( webser_t *webser, char * path, int str_len )
{
	int32 len = 0;
	char * ptr = path;

	len += snprintf( ptr+len, str_len-len, "%s", conf.http.home );
	len += snprintf( ptr+len, str_len-len, "%.*s", webser->http_req_head->uri.len, webser->http_req_head->uri.data );
    // if url is a dir, need to add index path
    if( webser->http_req_head->uri.data[webser->http_req_head->uri.len-1] == '/' )
	{
		len += snprintf( ptr+len, str_len-len, "%s", conf.http.index );
	}
}

static void webser_process_req_static_file_check ( webser_t * webser )
{
	struct stat st;
	int code = 200;
	char local_path[WEBSER_LENGTH_PATH_STR] = {0};

	webser_process_req_static_file_path( webser, local_path, WEBSER_LENGTH_PATH_STR );
	webser->file_mime = webser_get_mimetype( (unsigned char*)local_path, l_strlen(local_path) );
	
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
		webser->http_resp_code  = code;
		webser->filelen         = ( code == 200 ) ? st.st_size : 0;
        webser->filesend        = 0;
	} while(0);
	return;
}

static status webser_process_req_static_file ( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	if( (NULL == webser->http_resp_body) || !(webser->http_resp_body->callback_status & HTTP_BODY_STAT_OK) )
	{
		// static will discard all http request body
		if( OK != http_body_create( webser->c, &webser->http_resp_body, 1 ) )
		{
			err("http_body_create failed\n");
			webser_over( webser );
			return ERROR;
		}
		webser->http_resp_body->body_type 		= webser->http_req_head->content_type;
		webser->http_resp_body->content_length 	= webser->http_req_head->content_length;
		webser->http_resp_body->callback 		= webser_process_req_static_file;
		
		c->event->read_pt = webser_process_req_body;
		return c->event->read_pt( ev );
	}

	webser_process_req_static_file_check( webser );
	if( OK != webser_process_resp_head_build( webser ) ) 
	{
		err("webser resp head build failed\n");
		webser_over( webser );
		return ERROR;
	}
	
	if( webser->http_resp_code != 200 ) 
	{
		ev->read_pt = webser_response;
		return ev->read_pt( ev );
	}
	
	if( OK != webser_process_resp_body_static_file_build( webser ) ) 
	{
		err("webser resp body build failed\n");
		webser_over( webser );
		return ERROR;
	}
	ev->read_pt = webser_response;
	return ev->read_pt( ev );
}

static status webser_process_req_webapi( event_t * ev )
{
	int32 rc;
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	rc = webser->webapi_handler( ev );
	if( rc == AGAIN )
	{
		return rc;
	}

	webser->http_resp_code = ( rc == OK ) ? 200 : 500;
	if( OK != webser_process_resp_head_build( webser ) )
	{
		err("webser response head build failed\n");
		webser_over( webser );
		return ERROR;
	}
	ev->read_pt = webser_response;
	return ev->read_pt( ev );
}

static status webser_process_req_routing( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	status rc = serv_api_find( &webser->http_req_head->uri, &webser->webapi_handler );

	if( rc == OK )
	{
		webser->type 	= WEBSER_API;
		ev->read_pt 	= webser_process_req_webapi;
	}
	else
	{
		webser->type 	= WEBSER_STATIC;
		ev->read_pt		= webser_process_req_static_file;
	}
	return ev->read_pt( ev );
}

static status webser_process_http_req_header( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	status rc = 0;

	rc = webser->http_req_head->handler.process( webser->http_req_head );
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

	ev->read_pt	= webser_process_req_routing;
	return ev->read_pt( ev );
}

static status webser_cycle_init( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = NULL;

	if( NULL == c->meta ) 
	{
		if( OK != meta_alloc( &c->meta, WEBSER_REQ_META_LEN ) ) 
		{
			err("webser alloc con meta failed\n");
			net_free( c );
			return ERROR;
		}
	}
	if( OK != webser_alloc( &webser ) )
	{
		err( "webser alloc webser failed\n");
		net_free(c);
		return ERROR;
	}
	webser->c   = c;
	c->data     = webser;
	
	if( OK != http_request_head_create( c, &webser->http_req_head ) )
	{
		err("webser alloc request head failed\n");
		webser_over( webser );
		return ERROR;
	}
	ev->write_pt 	= NULL;
	ev->read_pt  	= webser_process_http_req_header;
	return ev->read_pt( ev );
}

static status webset_accept_callback_ssl( event_t * ev )
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
	ev->read_pt  	    = webser_cycle_init;
	return ev->read_pt( ev );
}

static status webser_accept_callback( event_t * ev )
{
	connection_t * c = ev->data;
	status rc = 0;

	do 
	{
		if( 1 == c->ssl_flag )
		{
			if( OK != ssl_create_connection( c, L_SSL_SERVER ) )
			{
				err("webser ssl con create failed\n");
				break;
			}
			rc = ssl_handshake( c->ssl );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					err("webser ssl handshake failed\n");
					break;
				}
				c->ssl->cb = webset_accept_callback_ssl;
				timer_set_data( &ev->timer, c );
				timer_set_pt( &ev->timer, webser_timeout_con );
				timer_add( &ev->timer, WEBSER_TIMEOUT );
				return AGAIN;
			}
			return webset_accept_callback_ssl( ev );
		}
		ev->read_pt = webser_cycle_init;
		return ev->read_pt( ev );
	} while(0);

	net_free( c );
	return ERROR;
}

status webser_init( void )
{
	uint32 i = 0;

	for( i = 0; i < conf.http.port_n; i ++ ) 
	{
		listen_add( conf.http.ports[i], webser_accept_callback, L_NOSSL );
	}
	for( i = 0; i < conf.http.ssl_portn; i ++ ) 
	{
		listen_add( conf.http.ssl_ports[i], webser_accept_callback, L_SSL );
	}
	
    if( this )
    {
        err("webser init this not empty\n");
        return ERROR;
    }
    this = l_safe_malloc(sizeof(private_webser_t)+MAX_NET_CON*sizeof(webser_t));
    if( !this )
    {
        err("webser alloc this failed, [%d]\n", errno );
        return ERROR;
    }
    memset(this, 0, sizeof(private_webser_t)+MAX_NET_CON*sizeof(webser_t));
    
	queue_init( &this->g_queue_use );
	queue_init( &this->g_queue_usable );
	for( i = 0; i < MAX_NET_CON; i ++ ) 
	{
		queue_insert_tail( &this->g_queue_usable, &this->g_pool[i].queue );
	}
	webapi_init();
	return OK;
}

status webser_end( void )
{
	if( this )
	{
		l_safe_free( this );
	}
	return OK;
}

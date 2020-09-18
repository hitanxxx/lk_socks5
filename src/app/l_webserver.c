#include "l_base.h"
#include "l_http_entitybody.h"
#include "l_http_request_head.h"
#include "l_http_response_head.h"
#include "l_webserver.h"
#include "l_webapi.h"

// private data
static queue_t 	g_queue_use;
static queue_t 	g_queue_usable;
static webser_t * g_pool = NULL;

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

static status webser_process_request_header( event_t * ev );
status webser_over( webser_t * webser );

status webser_set_mimetype( webser_t * webser, char * str, uint32 length )
{
	memcpy( webser->filepath, str, length );
	return OK;
}

static string_t * webser_get_mimetype( webser_t * webser )
{
	uint32 i;

	for( i =0; i < sizeof(mimetype_table)/sizeof(mime_type_t); i ++ ) 
	{
		if( NULL != l_find_str( webser->filepath, l_strlen(webser->filepath),mimetype_table[i].type.data, mimetype_table[i].type.len ) ) 
		{
			return &mimetype_table[i].header;
		}
	}
	err( "%s --- mimitype not found\n", __func__ );
	return &mimetype_table[0].header;
}

static status webser_alloc( webser_t ** webser )
{
	webser_t * new;
	queue_t * q;

	if( 1 == queue_empty( &g_queue_usable ) ) 
	{
		err( " don't have usbale\n");
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert_tail( &g_queue_use, q );
	new = l_get_struct( q, webser_t, queue );
	*webser = new;
	return OK;
}

static status webser_free( webser_t * webser )
{
	queue_remove( &webser->queue );
	queue_insert_tail( &g_queue_usable, &webser->queue );

	webser->c = NULL;
	webser->data = NULL;

	webser->request_head = NULL;
	webser->request_body = NULL;

	webser->api_handler = NULL;

	webser->filesize = 0;
	webser->re_status = 0;
	memset( webser->filepath, 0, sizeof(webser->filepath) );

	webser->ffd = 0;
	webser->response_head = NULL;
	webser->response_body = NULL;

	return OK;
}

static status webser_close( webser_t * webser )
{
	meta_t * meta, *cl;

	if( webser->request_head ) 
	{
		webser->request_head->handler.exit( webser->request_head );
	}
	
	if( webser->request_body ) 
	{
		webser->request_body->handler.exit( webser->request_body );
	}

	if( webser->ffd ) 
	{
		close( webser->ffd );
	}
	meta = webser->response_head;
	while( meta )
	{
		cl = meta->next;
		meta_free( meta );
		meta = cl;
	}
	webser_free( webser );
	return OK;
}

static inline void webser_timeout_con( void * data )
{
	net_free( (connection_t*)data );
}

static inline void webser_timeout_cycle( void * data )
{
	webser_over( (webser_t*)data );
}

static status webser_keepalive( webser_t * webser )
{
	connection_t* c;
	int32 rc;
	event_t * read;
	uint32 busy_length = 0;
	webser_t * new;

	c = webser->c;
	read = &c->event;
	if( webser->request_head->body_type == HTTP_BODY_TYPE_NULL )
	{
		busy_length = meta_len( c->meta->pos, c->meta->last );
		if( busy_length ) 
		{
			memcpy( c->meta->start, c->meta->pos, busy_length );
		}
	}
	c->meta->last = c->meta->pos = c->meta->start + busy_length;
	webser_close( webser );

	if( OK != webser_alloc( &new ) ) {
		err( " webser_alloc\n");
		return ERROR;
	}
	new->c = c;
	c->data = (void*)new;
	if( OK != http_request_head_create( c, &new->request_head ) ) 
	{
		err(" request create\n");
		webser_over( new );
		return ERROR;
	}
	c->event.write_pt = NULL;
	c->event.read_pt = webser_process_request_header;

	rc = c->event.read_pt( &c->event );
	if( rc == AGAIN )
	{
		timer_set_data( &c->event.timer, (void*)new );
		timer_set_pt( &c->event.timer, webser_timeout_cycle );
		timer_add( &c->event.timer, WEBSER_TIMEOUT );
	} 
	else if ( rc == ERROR ) 
	{
		webser_over( new );
	}
	return rc;
}

status webser_over( webser_t * webser )
{
	connection_t * c = webser->c;

	webser_close( webser );
	net_free( c );
	return OK;
}

static status webser_send_response( event_t * ev )
{
	status rc;
	connection_t * c;
	webser_t * webser;
	uint32 read_length;
	uint32 meta_len;
	meta_t * cl = NULL;

	c = ev->data;
	webser = c->data;
	while( 1 ) 
	{
		rc = c->send_chain( c, webser->response_head );
		if( rc == ERROR ) 
		{
			err("send response\n");
			webser_over( webser );
			return ERROR;
		} 
		else if( rc == DONE )
		{
			cl = webser->response_body;
			if( cl )
			{
				cl->file_pos += meta_len( cl->start, cl->pos );
				if( cl->file_pos > cl->file_last ) 
				{
					err(" file pos > file last\n");
					webser_over( webser );
					return ERROR;
				}
				
				if( cl->file_pos < cl->file_last )
				{
					cl->last = cl->pos = cl->start;
					meta_len = meta_len( webser->response_body->start, webser->response_body->end );
					rc = read( webser->ffd, cl->last, meta_len );
					if( rc <= 0 ) 
					{
						err( "read file data, errno [%d]\n", errno );
						webser_over( webser );
						return ERROR;
					}
					cl->last += rc;
					continue;
				}
			}
			timer_del( &c->event.timer );
			debug("send http response success\n");
			//if( webser->re_status == 200 && conf.http_keepalive && webser->request_head->keepalive_flag ) 
			if( webser->re_status == 200 && webser->request_head->keepalive_flag ) 
			{
				return webser_keepalive( webser );
			}
			return webser_over( webser );
		}
		
		timer_set_data( &c->event.timer, (void*)webser );
		timer_set_pt( &c->event.timer,  webser_timeout_cycle);
		timer_add( &c->event.timer, WEBSER_TIMEOUT );
		return rc;
	}
}

static status webser_test_reading( event_t * ev  )
{
	char buf[1];
	connection_t * c;
	webser_t * webser;
	socklen_t len;
	int err;
	ssize_t n;

	c = ev->data;
	webser = c->data;

	len = sizeof(int);
	if( getsockopt( c->fd, SOL_SOCKET, SO_ERROR, (void*)&err, &len ) == -1 )
	{
		err = errno;
	}
	goto closed;

	n = recv( c->fd, buf, 1, MSG_PEEK );
	if( n == -1 ) 
	{
		err( " recv errno [%d]\n", errno );
		goto closed;
	} 
	else if ( n == 0 ) 
	{
		err( " client close\n");
		goto closed;
	}
	return OK;

closed:
	webser_over( webser );
	return OK;
}

status webser_response( event_t * ev )
{
	connection_t* c;
	webser_t * webser;
	meta_t * cl;

	c = ev->data;
	webser = c->data;

	if( webser->response_body ) 
	{
		for( cl = webser->response_body; cl; cl = cl->next ) 
		{
			if( cl->file_pos == 0 && cl->file_last == 0 )
			{
				cl->file_last = meta_len( cl->pos, cl->last );
			}
		}
		webser->response_head->next = webser->response_body;
	}
	c->event.read_pt   = webser_test_reading;
	c->event.write_pt  = webser_send_response;
	event_opt( &c->event, c->fd, EV_W );
	return c->event.write_pt( ev );
}

static status webser_response_body_build( webser_t * webser )
{
	uint32 meta_len = 0;
	int32 rc = 0;
	
	webser->ffd = open( webser->filepath, O_RDONLY );
	if( webser->ffd == ERROR )
	{
		err( " open request file, errno [%d]\n", errno );
		return ERROR;
	}

	/* 
	 *  meta_file_alloc not alloc a memory meta, it will use sendfile
	 *  and ssl can't use sendfile function
	 */
	if( !webser->c->ssl_flag && (webser->filesize > WEBSER_BODY_META_LENGTH) ) 
	{
		if( OK != meta_file_alloc( &webser->response_body, webser->filesize ) )
		{
			err("meta file alloc\n" );
			return ERROR;
		}
	} 
	else 
	{
		meta_len = l_min( webser->filesize, WEBSER_BODY_META_LENGTH );
		if( OK != meta_alloc( &webser->response_body, meta_len ) ) 
		{
			err( "meta alloc response_body\n");
			return ERROR;
		}
		webser->response_body->file_pos = 0;
		webser->response_body->file_last = webser->filesize;

		rc = read( webser->ffd, webser->response_body->last, meta_len );
		if( rc <= 0 )
		{
			err( "read request file, errno [%d]\n", errno );
			return ERROR;
		}
		webser->response_body->last += rc;
	}
	return OK;
}

status webser_response_head_build( webser_t * webser )
{
	string_t * mimetype = NULL;
	char content_len_str[1024] = {0};
	uint32 head_len = 0, cur_len = 0;
	char * ptr;

	switch( webser->re_status )
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
	}

	if( webser->filesize > 0 )
	{
		head_len += snprintf( content_len_str, sizeof(content_len_str), "Content-Length: %d\r\n", webser->filesize );
		mimetype = webser_get_mimetype( webser );
		head_len += mimetype->len;
	}
	head_len += l_strlen("Server: lk-web-v1\r\n");
	head_len += l_strlen("Accept-Charset: utf-8\r\n");

	if( webser->request_head->headers_in.connection && ((uint32)webser->request_head->headers_in.connection->len > l_strlen("close")) )
	{
		head_len += l_strlen("Connection: keep-alive\r\n");
	} 
	else 
	{
		head_len += l_strlen("Connection: close\r\n");
	} 
	head_len += l_strlen("\r\n");


	if( OK != meta_alloc( &webser->response_head, head_len ) ) 
	{
		err( " meta_alloc response_head\n");
		return ERROR;
	}
	ptr = webser->response_head->data;

	switch( webser->re_status )
	{
		case 200:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 200 OK\r\n" );
			break;
		case 400:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 400 Bad Request\r\n" );
			break;
		case 404:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 404 Not Found\r\n" );
			break;
		case 500:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 500 Internal Server Error\r\n" );
			break;
		case 403:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 403 Forbidden\r\n" );
			break;
		default:
			cur_len += snprintf( ptr + cur_len, head_len - cur_len, "HTTP/1.1 400 Bad Request\r\n" );
	}
	
	cur_len += snprintf( ptr + cur_len, head_len - cur_len, "Server: lk-web-v1\r\n" );
	cur_len += snprintf( ptr + cur_len, head_len - cur_len, "Accept-Charset: utf-8\r\n" );
	if( webser->request_head->headers_in.connection && ((uint32)webser->request_head->headers_in.connection->len > l_strlen("close")) )
	{
		cur_len += snprintf( ptr + cur_len, head_len - cur_len, "Connection: keep-alive\r\n" );
	}
	else
	{
		cur_len += snprintf( ptr + cur_len, head_len - cur_len, "Connection: close\r\n" );
	}
	
	if( webser->filesize > 0 )
	{
		cur_len += snprintf( ptr + cur_len, head_len - cur_len, "%s", content_len_str );
		cur_len += snprintf( ptr + cur_len, head_len - cur_len, "%.*s", mimetype->len, mimetype->data );
	}
	cur_len += snprintf( ptr + cur_len, head_len - cur_len, "\r\n" );

	if( cur_len != head_len )
	{
		err("header build failed, curlen [%d] head_len [%d]\n", cur_len, head_len );
		return ERROR;
	}
	webser->response_head->last += head_len;
	return OK;
}

static status webser_process_static_check_file ( webser_t * webser )
{
	struct stat st;
	char * ptr;
	uint32 length;

	ptr = webser->filepath;
	
	/* 
	 *  always cut conf home's last char when last char is '/' 
	 */
	length = ( conf.http.home[ l_strlen(conf.http.home) ] == '/' ) ? (l_strlen(conf.http.home) - 1) : l_strlen(conf.http.home);
	memcpy( ptr, conf.http.home, length );
	ptr += length;

	/*
	 *  url frist char always have '/' 
	 */
	memcpy( ptr, webser->request_head->uri.data, webser->request_head->uri.len);
	ptr += webser->request_head->uri.len;

	/* 
	 *  if url is dir then need to add index suffix 
	 */
	if( webser->request_head->uri.data[webser->request_head->uri.len-1] == '/' )
	{
		memcpy( ptr, conf.http.index, l_strlen(conf.http.index) );
		ptr += l_strlen(conf.http.index);
	}

	if( OK != stat( webser->filepath, &st ) ) 
	{
		err("webser stat check file [%s] failed, [%d]\n", webser->filepath, errno );
		webser->re_status = 400;
		return OK;
	}
	if( S_ISREG(st.st_mode) ) 
	{
		webser->re_status = ( st.st_mode & S_IRUSR ) ? 200 : 403;
	} 
	else
	{
		webser->re_status = 404;
	}
	webser->filesize = ( webser->re_status == 200 ) ? (uint32)st.st_size : 0;
	return OK;
}

status webser_process_request_body( event_t * ev )
{
	status rc;
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	rc = webser->request_body->handler.process( webser->request_body );
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
	return webser->request_body->over_cb( ev );
}


static status webser_process_static ( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	if( (NULL == webser->request_body) || !(webser->request_body->status & HTTP_BODY_STAT_OK) )
	{
		// static not support post
		if( OK != http_entitybody_create( webser->c, &webser->request_body, 1 ) )
		{
			err( "http_entitybody_create error\n");
			webser_over( webser );
			return ERROR;
		}
		webser->request_body->body_type 		= webser->request_head->body_type;
		webser->request_body->content_length 	= webser->request_head->content_length;
		webser->request_body->over_cb 			= webser_process_static;
		
		c->event.read_pt = webser_process_request_body;
		return c->event.read_pt( ev );
	}

	if( OK != webser_process_static_check_file( webser ) )
	{
		err("webser static check file failed\n");
		webser_over( webser );
		return ERROR;
	}
	
	if( OK != webser_response_head_build( webser ) ) 
	{
		err("webser resp head build failed\n");
		webser_over( webser );
		return ERROR;
	}
	
	if( webser->re_status != 200 ) 
	{
		c->event.read_pt = webser_response;
		return c->event.read_pt( &c->event );
	}
	
	if( OK != webser_response_body_build( webser ) ) 
	{
		err("webser resp body build failed\n");
		webser_over( webser );
		return ERROR;
	}
	c->event.read_pt = webser_response;
	return c->event.read_pt( ev );
}

static status webser_process_webapi( event_t * ev )
{
	int32 rc;
	connection_t * c = ev->data;
	webser_t * webser = c->data;

	rc = webser->api_handler( ev );
	if( rc == OK || rc == ERROR )
	{
		if( rc == OK )
		{	
			webser->re_status = 200;
			debug("webser webapi process success\n");
		}
		else if ( rc == ERROR )
		{
			webser->re_status = 500;
			err("webser webapi process failed\n");
		}
		
		if( OK != webser_response_head_build( webser ) )
		{
			err("webser response head build failed\n");
			webser_over(webser);
			return ERROR;
		}
		c->event.read_pt = webser_response;
		return c->event.read_pt( &c->event );
	}	
	return rc;
}

static status webser_process_routing( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	
	if( OK == serv_api_find( &webser->request_head->uri, &webser->api_handler ) ) 
	{
		c->event.read_pt = webser_process_webapi;
	} 
	else 
	{
		c->event.read_pt = webser_process_static;
	}
	return c->event.read_pt( ev );
}

static status webser_process_request_header( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * webser = c->data;
	status rc;

	rc = webser->request_head->handler.process( webser->request_head );
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

	ev->read_pt	= webser_process_routing;
	return ev->read_pt( ev );
}

static status webser_cycle_init( event_t * ev )
{
	connection_t * c = ev->data;
	webser_t * local_webser;

	if( NULL == c->meta ) 
	{
		if( OK != meta_alloc( &c->meta, WEBSER_REQ_META_LEN ) ) 
		{
			err("webser con meta alloc failed\n");
			net_free( c );
			return ERROR;
		}
	}
	if( OK != webser_alloc( &local_webser ) ) 
	{
		err( "webser alloc webser failed\n");
		net_free(c);
		return ERROR;
	}
	local_webser->c = c;
	c->data = local_webser;
	
	if( OK != http_request_head_create( c, &local_webser->request_head ) ) 
	{
		err("webser alloc request head failed\n");
		webser_over( local_webser );
		return ERROR;
	}
	c->event.write_pt 	= NULL;
	c->event.read_pt  	= webser_process_request_header;
	return c->event.read_pt( ev );
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
	timer_del( &c->event.timer );
	c->recv 			= ssl_read;
	c->send 			= ssl_write;
	c->recv_chain 		= NULL;
	c->send_chain 		= ssl_write_chain;

	c->event.write_pt 	= NULL;
	c->event.read_pt  	= webser_cycle_init;
	return c->event.read_pt( ev );
}

static status webser_accept_callback( event_t * ev )
{
	connection_t * c = ev->data;
	status rc;

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

	if( c->ssl_flag ) 
	{
		if( OK != ssl_create_connection( c, L_SSL_SERVER ) ) 
		{
			err( " ssl create\n");
			net_free( c );
			return ERROR;
		}
		rc = ssl_handshake( c->ssl );
		if( rc == ERROR )
		{
			err( " ssl handshake\n");
			net_free( c );
			return ERROR;
		} 
		else if ( rc == AGAIN ) 
		{
			timer_set_data( &c->event.timer, (void*)c );
			timer_set_pt( &c->event.timer, webser_timeout_con );
			timer_add( &c->event.timer, WEBSER_TIMEOUT );

			c->ssl->cb = webset_accept_callback_ssl;
			return AGAIN;
		}
		return webset_accept_callback_ssl( ev );
	}
	c->event.read_pt = webser_cycle_init;
	
	return webser_cycle_init( ev );
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
	
	queue_init( &g_queue_use );
	queue_init( &g_queue_usable );
	g_pool = ( webser_t *) l_safe_malloc( sizeof(webser_t) * MAXCON );
	if( !g_pool ) 
	{
		err( " l_safe_malloc g_pool\n" );
		return ERROR;
	}
	memset( g_pool, 0, sizeof(webser_t) * MAXCON );
	for( i = 0; i < MAXCON; i ++ ) 
	{
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}

	webapi_init();
	return OK;
}

status webser_end( void )
{
	if( g_pool ) 
	{
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}

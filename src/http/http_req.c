#include "common.h"
#include "http_req.h"

static queue_t 	g_queue_use;
static queue_t 	g_queue_usable;
http_req_t * g_pool = NULL;

static status http_req_set_value( http_req_t * request, string_t * value, uint32 offsetof );
static status http_req_set_connection( http_req_t * request, string_t * value, uint32 offsetof );

static http_req_value_t headers[] = 
{
	{ 	string("Host"),				http_req_set_value,			offsetof(http_req_headers_t, host) },
	{ 	string("Content-Length"),	http_req_set_value,			offsetof(http_req_headers_t, content_length) },
	{ 	string("Connection"),		http_req_set_connection,	offsetof(http_req_headers_t, connection)	},
	{ 	string("Transfer-Encoding"),http_req_set_value,			offsetof(http_req_headers_t, transfer_encoding)	},
	{   string("User-Agent"),		http_req_set_value,			offsetof(http_req_headers_t, user_agent) }
};

status http_req_have_body(     http_req_t * req)
{
	if( (0 != strncmp( (char*)req->method.data, "POST", req->method.len )) &&
		(0 != strncmp( (char*)req->method.data, "PUT", req->method.len )) )
	{
		debug("http req method [%.*s] not allow have body\n", req->method.len, req->method.data );
		return 0;
	}

	if( req->content_type == HTTP_BODY_TYPE_CONTENT ||
		req->content_type == HTTP_BODY_TYPE_CHUNK  )
	{
		return 1;
	}
	return 0;
}

static status http_req_set_value( http_req_t * request, string_t * value, uint32 offsetof )
{
	string_t ** h = (string_t**) ((void*) &request->headers + offsetof );
	*h = value;
	return OK;
}

static status http_req_set_connection( http_req_t * request, string_t * value, uint32 offsetof )
{
	string_t ** h = (string_t**) ((void*) &request->headers + offsetof );;
	*h = value;

	request->keepalive = (value->len > (int32)l_strlen("close")) ? 1 : 0;
	return OK;
}

static status http_req_find_header( string_t * str, http_req_value_t ** header )
{
	int32 i = 0;
	for( i = 0; i < (int32)(sizeof(headers)/sizeof(headers[0])); i ++ ) 
	{
		if( headers[i].name.len == str->len ) 
		{
			if( OK == strncasecmp( (char*)headers[i].name.data, (char*)str->data, (int)str->len ) ) 
			{
				*header = &headers[i];
				return OK;
			}
		}
	}
	return ERROR;
}

static status http_req_recv( connection_t * c, meta_t * meta )
{
	ssize_t rc = 0;

	if( meta_len( meta->pos, meta->last ) > 0 ) 
	{
		return OK;
	}
	rc = c->recv( c, meta->last,meta_len( meta->last, meta->end ) );
	if( rc < 0 )
	{
        if( rc == ERROR ) return ERROR;
        return AGAIN;
	}
	meta->last += rc;
	return OK;
}



static status http_req_parse_header_line( http_req_t * request, meta_t * meta )
{
	unsigned char *p = NULL;

	enum {
		s_start = 0,
		s_name,
		s_name_after,
		s_value,
		s_almost_done,
		s_header_end_r
	} state;

	state = request->state;
	for( p = meta->pos; p < meta->last; p ++ ) 
	{
		switch( state ) 
		{
			case	s_start:
				if( ( *p >= 32 && *p <= 126 ) || *p == '\r' ) 
				{
					request->header_key.data = p;
					if( *p == '\r') 
					{
						state = s_header_end_r;
						break;
					}
					else
					{
						state = s_name;
						break;
					}
				} 
				else 
				{
					err("http req header line s_start illegal [%d]\n", *p );
					return ERROR;
				}
			case	s_name:
				if(( *p >= 32 && *p <= 126 ) ) 
				{
					if( *p == ':' ) 
					{
						request->header_key.len = meta_len( request->header_key.data, p );
						state = s_name_after;
						break;
					}
				} 
				else 
				{
					err ("http req header s_name illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_name_after:
				if( *p == ' ' )
				{
					break;
				}
				else
				{
					request->header_value.data = p;
					state = s_value;
				}
			case	s_value:
				if( *p == '\r' ) 
				{
					request->header_value.len = meta_len( request->header_value.data, p );
					state = s_almost_done;
				}
				break;
			case	s_almost_done:
				if( *p != '\n' ) 
				{
					err("http req s_almost_done not '\n'\n" );
					return ERROR;
				}
				goto  done;
			case 	s_header_end_r:
				if( *p != '\n' ) 
				{
					err("http req s_header_end_r not '\n'\n" );
					return ERROR;
				}
				goto  header_done;
		}
	}
	meta->pos 		= p;
	request->state 	= state;
	return AGAIN;
done:
	meta->pos 		= p+1;
	request->state 	= 0;
	return OK;
header_done:
	meta->pos 		= p+1;
	request->state 	= 0;
	return DONE;
}

static status http_req_proc_headers( http_req_t * req )
{
	int rc = AGAIN;
	connection_t * c = req->c;
	http_req_value_t * headers;
	string_t  *str_name, *str_value;

	if( OK != mem_arr_create( &req->headers.list, sizeof(string_t) ) )
	{
		err("http req headers list create failed\n");
		return ERROR;
	}
		
	while( 1 ) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_req_recv( c, c->meta );
			if( rc < 0 )
			{
				return rc;
			}
		}
		
		rc = http_req_parse_header_line( req, c->meta );
		if( rc == OK ) 
		{
			str_name 				= &req->header_key;
			if( OK == http_req_find_header( str_name, &headers ) )
			{
				str_value 			= (string_t*)mem_arr_push( req->headers.list );
				str_value->data 	= req->header_value.data;
				str_value->len 		= req->header_value.len;
				if( headers->handler ) 
				{
					headers->handler( req, str_value, headers->offsetof );
				}
			}
			continue;
		} 
		else if( rc == DONE ) 
		{
			if( req->headers.user_agent ) 
			{
				access_log("%.*s - %s - %.*s\n", 
					meta_len( req->request_line_start, req->request_line_end ),
					req->request_line_start,
					inet_ntoa( req->c->addr.sin_addr ),
					req->headers.user_agent->len, req->headers.user_agent->data );			
			}
			else
			{
				access_log("%.*s - %s - null\n", 
					meta_len( req->request_line_start, req->request_line_end ),
					req->request_line_start,
					inet_ntoa( req->c->addr.sin_addr ) );	
			}

			// process req body info
			req->content_type = HTTP_BODY_TYPE_NULL;
			if ( req->headers.transfer_encoding ) 
			{
				req->content_type = HTTP_BODY_TYPE_CHUNK;
			} 
			else if( req->headers.content_length ) 
			{
				req->content_type 	= HTTP_BODY_TYPE_CONTENT;
				char num_str[64] = {0};
				
				memset( num_str, 0, sizeof(num_str) ); 
				memcpy( num_str, req->headers.content_length->data, req->headers.content_length->len );
				req->content_length = strtol( num_str, NULL, 10 );
				debug("content-length [%d]\n", req->content_length );
				if( req->content_length <= 0 )
				{
					req->content_type = HTTP_BODY_TYPE_NULL;
				}
			} 
			return DONE;
		} 
		else if( rc == ERROR )
		{
			err("http req parse header line");
			return ERROR;
		}
		
		if( c->meta->last == c->meta->end ) 
		{
			err("http req headers too large");
			return ERROR;
		}
	}
}

static status http_req_parse_request_line( http_req_t * req, meta_t * meta )
{
	unsigned char *p = NULL;

	enum {
		s_start = 0,
		s_method,
		s_after_method,
		s_schema,
		s_schema_slash,
		s_schema_slash_slash,
		s_host_start,
		s_host,
		s_colon,
		s_port,
		s_uri,
		s_after_uri,
		s_version,
		s_end
	} state;

	state = req->state;
	for( p = meta->pos; p < meta->last; p ++ )
	{	
		switch( state ) 
		{
			case	s_start:
				if( ( *p >= 'A' && *p <= 'Z' ) || *p == '\r' || *p == '\n' || *p == ' ')
				{
					if(*p == '\r' ||*p == '\n' ||*p == ' ' )
					{
						break;
					}
					req->request_line_start 	= p;
					req->method.data 			= p;
					state = s_method;
				}
				break;
			case	s_method:
				if( ( *p >= 'A' && *p <= 'Z' ) || *p == ' ' ) 
				{
					if( *p == ' ' )
					{
						req->method.len = meta_len( req->method.data, p );
						if( req->method.len < 1 || req->method.len > 8 )
						{
							err("http req method len [%d] illegal, (1-8)\n", req->method.len );
							return ERROR;
						}
						state = s_after_method;
					}
				} 
				else 
				{
					err("http req method illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_after_method:
				if(( *p >= 'a' && *p <= 'z' ) ||*p == '/' ||*p == ' ' ) 
				{
					if( *p >= 'a' && *p <= 'z' ) 
					{
						req->schema.data = p;
						state = s_schema;
					} 
					else if ( *p == '/' ) 
					{
						req->uri.data = p;
						state = s_uri;
					}
				} 
				else 
				{
					err("http req after method illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_schema:
				if(( *p >= 'a' && *p <= 'z' ) ||*p == ':' ) 
				{
					if(  *p == ':' )
					{
						req->schema.len = meta_len( req->schema.data, p );
						state = s_schema_slash;
					}
				} 
				else 
				{
					err("http req schema error [%c]\n", *p );
					return ERROR;
				}
				break;
			case	s_schema_slash:
				if( *p == '/' ) 
				{
					state = s_schema_slash_slash;
				} 
				else 
				{
					err("http req schema slash not '/'\n" );
				}
				break;
			case	s_schema_slash_slash:
				if( *p == '/' ) 
				{
					state = s_host_start;
				} 
				else 
				{
					err("htto req schema slash slash not '/'\n" );
				}
				break;
			case	s_host_start:
				req->host.data = p;
				state = s_host;
			case	s_host:
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= 'A' && *p <= 'Z' ) ||
				( *p >= '0' && *p <= '9' ) ||
				*p == '.' ||
				*p == '/' ||
				*p == ':' ||
				*p == '-' ||
				*p == '_' )
				{
					if( *p == '/' ) 
					{
						req->host.len = meta_len( req->host.data, p );
						req->uri.data = p;
						state = s_uri;
					} 
					else if ( *p == ':' )
					{
						req->host.len = meta_len( req->host.data, p );
						state = s_colon;
					}
				} 
				else 
				{
					err("http req host illegal [%d]\n",*p );
					return ERROR;
				}
				break;
			case s_colon:
				if( *p >= '0' && *p <= '9' )
				{
					req->port.data = p;
					state = s_port;
				} 
				else 
				{
					err("http req colon illegal [%d]\n", *p );
					return ERROR;
				}
			case  s_port:
				if((*p >= '0' && *p <= '9' ) ||*p == ' ' ||*p == '/' ) 
				{
					if( *p == ' ' ) 
					{
						req->port.len = meta_len( req->port.data, p );
						state = s_after_uri;
					} 
					else if ( *p == '/' ) 
					{
						req->port.len = meta_len( req->port.data, p );
						req->uri.data = p;
						state = s_uri;
					}
				} 
				else 
				{
					err("http req port illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_uri:
				if( *p == ' ' ) 
				{
					req->uri.len = meta_len( req->uri.data, p );
					if( req->uri.len > REQ_LENGTH_URI_STR )
					{
						err("http req header uri too long\n");
						return ERROR;
					}
					state = s_after_uri;
				}
				break;
			case	s_after_uri:
				if(( *p >= 'a' && *p <= 'z' ) ||( *p >= 'A' && *p <= 'Z' ) ||*p == ' ' )
				{
					if((*p >= 'a' && *p <= 'z') ||( *p >= 'A' && *p <= 'Z' )) 
					{
						req->http_version.data = p;
						state = s_version;
					}
				} 
				else 
				{
					err("http req uri illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_version:
				if( meta_len( req->http_version.data, p ) > 8 )  
				{
					err("http req version more than 8\n" );
					return ERROR;
				}
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= 'A' && *p <= 'Z' ) ||
				( *p >= '0' && *p <= '9' ) ||
				*p == '/' ||
				*p == '.' ||
				*p == '\r' ) 
				{
					if( *p == '\r' )
					{
						req->request_line_end = p;
						req->http_version.len = meta_len( req->http_version.data, p );
						state = s_end;
					}
				} 
				else 
				{
					err("http req version illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case	s_end:
				if( *p != '\n' ) 
				{
					err("http req end illegal [%d]\n", *p );
					return ERROR;
				}
				goto done;
				break;
		}
	}
	meta->pos 	= p;
	req->state 	= state;
	return AGAIN;
done:
	meta->pos 	= p + 1;
	req->state 	= 0;
	return DONE;
}


static status http_req_proc_request_line( http_req_t * req )
{
	int32 rc = AGAIN;
	connection_t * c = req->c;
	
	while( 1 ) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_req_recv( c, c->meta );
			if( rc < 0 )
			{
				return rc;
			}
		}
		rc = http_req_parse_request_line( req, c->meta );
		if( rc == DONE ) 
		{
			req->cb 					= http_req_proc_headers;
			return req->cb( req );
		} 
		else if( rc == ERROR ) 
		{
			err("http req parse line failed\n");
			return ERROR;
		}
		
		// request header size muse less than meta's space
		if( c->meta->last == c->meta->end ) 
		{
			err("http request line too long\n");
			return ERROR;
		}
	}
}

static status http_req_alloc( http_req_t ** req )
{
	http_req_t * new;
	queue_t * q;

	if( queue_empty( &g_queue_usable ) == 1 ) 
	{
		err("http req g_queue_usable empty\n");
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert_tail( &g_queue_use, q );
	new = ptr_get_struct( q, http_req_t, queue );

	*req = new;
	return OK;
}

inline static void http_req_clear_string( string_t * str)
{
	str->len 	= 0;
	str->data 	= NULL;
}

status http_req_free( http_req_t * req )
{
	req->c 					= NULL;
	req->cb 				= NULL;
	req->state 				= 0;

	req->request_line_start = NULL;
	req->request_line_end 	= NULL;
	http_req_clear_string( &req->method );
	http_req_clear_string( &req->schema );
	http_req_clear_string( &req->host );
	http_req_clear_string( &req->uri );
	http_req_clear_string( &req->port );
	http_req_clear_string( &req->http_version );
	
	http_req_clear_string( &req->header_key );
	http_req_clear_string( &req->header_value );
	
	if( req->headers.list ) 
	{
		mem_arr_free( req->headers.list );
        memset( &req->headers, 0, sizeof(http_req_headers_t) );
	}
	req->keepalive 			= 0;
	req->content_length 	= 0;
	req->content_length 	= 0;
	
	queue_remove( &req->queue );
	queue_insert_tail( &g_queue_usable, &req->queue );
	return OK;
}


status http_req_create( connection_t * c, http_req_t ** request )
{
	http_req_t * new_req = NULL;

	if( OK != http_req_alloc(&new_req) )
	{
		err("http req alloc new failed\n");
		return ERROR;
	}
	new_req->cb 		= http_req_proc_request_line;
	
	new_req->c 			= c;
	new_req->state 		= 0;
	*request 			= new_req;
	return OK;
}

status http_req_init_module( void )
{
	uint32 i;

	queue_init( &g_queue_use );
	queue_init( &g_queue_usable );
	g_pool = (http_req_t*)l_safe_malloc( sizeof(http_req_t)*MAX_NET_CON );
	if( !g_pool ) 
	{
		err("http req header malloc g_pool failed\n");
		return ERROR;
	}
	memset( g_pool, 0, sizeof(http_req_t)*MAX_NET_CON );
	for( i = 0; i < MAX_NET_CON; i++ ) 
	{
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}
	return OK;
}

status http_req_end_module( void )
{
	if( g_pool ) 
	{
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}

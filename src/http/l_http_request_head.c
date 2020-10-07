#include "l_base.h"
#include "l_http_request_head.h"

static queue_t 	g_queue_use;
static queue_t 	g_queue_usable;
http_req_head_t * g_pool = NULL;

static status http_request_head_set_value( http_req_head_t * request, string_t * value, uint32 offsetof );
static status http_request_head_set_connection( http_req_head_t * request, string_t * value, uint32 offsetof );

static header_t headers[] = 
{
	{ 	string("Host"),				http_request_head_set_value,		offsetof(http_req_headers_in_t, host) },
	{ 	string("Content-Length"),	http_request_head_set_value,		offsetof(http_req_headers_in_t, content_length) },
	{ 	string("Connection"),		http_request_head_set_connection,	offsetof(http_req_headers_in_t,	connection)	},
	{ 	string("Transfer-Encoding"),http_request_head_set_value,		offsetof(http_req_headers_in_t,	transfer_encoding)	},
	{   string("User-Agent"),		http_request_head_set_value,		offsetof(http_req_headers_in_t,	user_agent) }
};

static status http_request_head_set_value( http_req_head_t * request, string_t * value, uint32 offsetof )
{
	string_t ** h = (string_t**) ((void*) &request->headers_in + offsetof );
	*h = value;
	return OK;
}

static status http_request_head_set_connection( http_req_head_t * request, string_t * value, uint32 offsetof )
{
	string_t ** h = (string_t**) ((void*) &request->headers_in + offsetof );;
	*h = value;

	request->keepalive = (value->len > (int32)l_strlen("close")) ? 1 : 0;
	return OK;
}

static status http_request_head_find_header( string_t * str, header_t ** header )
{
	int32 i = 0;
	for( i = 0; i < (int32)(sizeof(headers)/sizeof(headers[0])); i ++ ) 
	{
		if( headers[i].name.len == str->len ) 
		{
			if( OK == l_strncmp_cap( headers[i].name.data, headers[i].name.len, str->data, str->len ) ) 
			{
				*header = &headers[i];
				return OK;
			}
		}
	}
	return ERROR;
}

static status http_request_head_recv( connection_t * c, meta_t * meta )
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

static status http_request_head_parse_request_line( http_req_head_t * request, meta_t * meta )
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

	state = request->state;
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
					request->method.data = p;
					state = s_method;
				}
				break;
			case	s_method:
				if( ( *p >= 'A' && *p <= 'Z' ) || *p == ' ' ) 
				{
					if( *p == ' ' )
					{
						request->method.len = meta_len( request->method.data, p );
						if( request->method.len > 8 ) 
						{
							err("http req method > 8\n" );
							return ERROR;
						} 
						else if ( request->method.len < 1  ) 
						{
							err("http req method length < 1\n" );
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
						request->schema.data = p;
						state = s_schema;
					} 
					else if ( *p == '/' ) 
					{
						request->uri.data = p;
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
						request->schema.len = meta_len( request->schema.data, p );
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
				request->host.data = p;
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
						request->host.len = meta_len( request->host.data, p );
						request->uri.data = p;
						state = s_uri;
					} 
					else if ( *p == ':' )
					{
						request->host.len = meta_len( request->host.data, p );
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
					request->port.data = p;
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
						request->port.len = meta_len( request->port.data, p );
						state = s_after_uri;
					} 
					else if ( *p == '/' ) 
					{
						request->port.len = meta_len( request->port.data, p );
						request->uri.data = p;
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
					request->uri.len = meta_len( request->uri.data, p );
					state = s_after_uri;
				}
				break;
			case	s_after_uri:
				if(( *p >= 'a' && *p <= 'z' ) ||( *p >= 'A' && *p <= 'Z' ) ||*p == ' ' )
				{
					if((*p >= 'a' && *p <= 'z') ||( *p >= 'A' && *p <= 'Z' )) 
					{
						request->http_version.data = p;
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
				if( meta_len( request->http_version.data, p ) > 8 )  
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
						request->request_line_end = p;
						request->http_version.len = meta_len( request->http_version.data, p );
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
	meta->pos = p;
	request->state = state;
	return AGAIN;
done:
	meta->pos = p + 1;
	request->state = 0;
	return DONE;
}

static status http_request_head_parse_header_line( http_req_head_t * request, meta_t * meta )
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

static status http_request_head_process_headers( http_req_head_t * request )
{
	int rc = AGAIN;
	connection_t * c = request->c;
	header_t * headers;
	string_t  *str_name, *str_value;
	int32 content_length;
	string_t user_agent = string("empty");

	if( OK != mem_list_create( &request->headers_in.list, sizeof(string_t) ) )
	{
		err ("http req headers_in list create failed\n");
		return ERROR;
	}
		
	while( 1 ) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_request_head_recv( c, c->meta );
			if( rc < 0 )
			{
				return rc;
			}
		}
		
		rc = http_request_head_parse_header_line( request, c->meta );
		if( rc == OK ) 
		{
            //debug("[%.*s] [%.*s]\n", request->header_key.len, request->header_key.data, request->header_value.len, request->header_value.data );
			str_name 				= &request->header_key;
			if( OK == http_request_head_find_header( str_name, &headers ) )
			{
				str_value 			= (string_t*)mem_list_push( request->headers_in.list );
				str_value->data 	= request->header_value.data;
				str_value->len 		= request->header_value.len;
				if( headers->handler ) 
				{
					headers->handler( request, str_value, headers->offsetof );
				}
			}
			continue;
		} 
		else if( rc == DONE ) 
		{
			if( request->headers_in.user_agent ) 
			{
				user_agent.len 		= request->headers_in.user_agent->len;
				user_agent.data 	= request->headers_in.user_agent->data;
			}
			access_log("%.*s - %s - %.*s\n", meta_len( request->request_line_start, request->request_line_end ), request->request_line_start, inet_ntoa( request->c->addr.sin_addr ), user_agent.len, user_agent.data );

			if( request->headers_in.content_length ) 
			{
				request->content_type 	= HTTP_BODY_TYPE_CONTENT;
				if( OK != l_atoi( request->headers_in.content_length->data, request->headers_in.content_length->len, &content_length ) )
			 	{
					err("http req get content length number failed\n");
					return ERROR;
				}
				request->content_length = (uint32)content_length;
			} 
			else if ( request->headers_in.transfer_encoding ) 
			{
				request->content_type = HTTP_BODY_TYPE_CHUNK;
			} 
			else 
			{
				request->content_type = HTTP_BODY_TYPE_NULL;
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

static status http_request_head_process_request_line( http_req_head_t * request )
{
	int32 rc = AGAIN;
	connection_t * c = request->c;
	
	while( 1 ) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_request_head_recv( c, c->meta );
			if( rc < 0 )
			{
				return rc;
			}
		}
		rc = http_request_head_parse_request_line( request, c->meta );
		if( rc == DONE ) 
		{
			if( request->uri.len > REQ_LENGTH_URI_STR )
			{
				err("http req header uri too long\n");
				return ERROR;
			}
			request->request_line_start 	= c->meta->start;
			request->handler.process 		= http_request_head_process_headers;
			return request->handler.process( request );
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

static status http_request_head_alloc( http_req_head_t ** request )
{
	http_req_head_t * new;
	queue_t * q;

	if( queue_empty( &g_queue_usable ) == 1 ) 
	{
		err("http req header g_queue_usable empty\n");
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert_tail( &g_queue_use, q );
	new = l_get_struct( q, http_req_head_t, queue );

	*request = new;
	return OK;
}

inline static void http_request_head_clear_string( string_t * str)
{
	str->len 	= 0;
	str->data 	= NULL;
}

static status http_request_head_free( http_req_head_t * request )
{
	request->c 					= NULL;
	request->handler.exit 		= NULL;
	request->handler.process 	= NULL;
	request->state 				= 0;

	request->request_line_start = NULL;
	request->request_line_end 	= NULL;
	http_request_head_clear_string( &request->method );
	http_request_head_clear_string( &request->schema );
	http_request_head_clear_string( &request->host );
	http_request_head_clear_string( &request->uri );
	http_request_head_clear_string( &request->port );
	http_request_head_clear_string( &request->http_version );
	
	http_request_head_clear_string( &request->header_key );
	http_request_head_clear_string( &request->header_value );
	
	if( request->headers_in.list ) 
	{
		mem_list_free( request->headers_in.list );
		request->headers_in.list = NULL;
	}
	request->keepalive 			= 0;
	request->content_length 	= 0;
	request->content_length 	= 0;
	
	queue_remove( &request->queue );
	queue_insert_tail( &g_queue_usable, &request->queue );
	return OK;
}


status http_request_head_create( connection_t * c, http_req_head_t ** request )
{
	http_req_head_t * local_header;

	if( OK != http_request_head_alloc(&local_header) )
	{
		err("http header alloc request failed\n");
		return ERROR;
	}
	local_header->handler.process = http_request_head_process_request_line;
	local_header->handler.exit    = http_request_head_free;
	
	local_header->c 		= c;
	local_header->state 	= 0;
	*request = local_header;
	return OK;
}

status http_request_head_init_module( void )
{
	uint32 i;

	queue_init( &g_queue_use );
	queue_init( &g_queue_usable );
	g_pool = (http_req_head_t*)l_safe_malloc( sizeof(http_req_head_t)*MAX_NET_CON );
	if( !g_pool ) 
	{
		err("http req header malloc g_pool failed\n");
		return ERROR;
	}
	memset( g_pool, 0, sizeof(http_req_head_t)*MAX_NET_CON );
	for( i = 0; i < MAX_NET_CON; i++ ) 
	{
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}
	return OK;
}

status http_request_head_end_module( void )
{
	if( g_pool ) 
	{
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}

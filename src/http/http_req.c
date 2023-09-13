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
	/// if not POST and not PUT method, then don't need have body
	if( (0 != strncmp( (char*)req->method.data, "POST", req->method.len )) &&
		(0 != strncmp( (char*)req->method.data, "PUT", req->method.len )) ) {
		//debug("http req method [%.*s] not allow have body\n", req->method.len, req->method.data );
		return 0;
	}

	/// if have content header, means have body
	if( req->content_type == HTTP_BODY_TYPE_CONTENT ||
		req->content_type == HTTP_BODY_TYPE_CHUNK  ) {
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
	for( i = 0; i < (int32)(sizeof(headers)/sizeof(headers[0])); i ++ ) {
		if( headers[i].name.len == str->len ) {
			if( OK == strncasecmp( (char*)headers[i].name.data, (char*)str->data, (int)str->len ) ) {
				*header = &headers[i];
				return OK;
			}
		}
	}
	return ERROR;
}

static status http_req_recv( connection_t * c, meta_t * meta )
{
	ssize_t recvn = 0;

	if( meta_len( meta->pos, meta->last ) > 0 ) {
		return OK;
	}
	recvn = c->recv( c, meta->last, meta_len( meta->last, meta->end ) );
	if( recvn < 0 ) {
        if( recvn == ERROR ) {
			return ERROR;
		}
        return AGAIN;
	}
	meta->last += recvn;
	return OK;
}



static status http_req_headers_analysis( http_req_t * req, meta_t * meta )
{
	unsigned char *p = NULL;
	int line_end = 0;
	int all_end = 0;

	enum {
		s_key_init = 0,
		s_key,
		s_value_init,
		s_value,
		s_end,		/// end means line end, \r\n
		s_done		/// done means header finish, \r\n\r\n
	} state;

	state = req->state;
	for( p = meta->pos; p < meta->last; p ++ ) {

		if( state == s_key_init ) {
			if( (*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '-' || *p == '@' ||
				*p == '_'
			) {
				req->header_k.data = p;
				state = s_key;
				continue;
			} else if ( *p == CR ) {
				state = s_done;
				continue;
			} else {
				err("req headers. s_key_init illegal [%c]\n",  *p );
				return ERROR;
			}
		}

		if( state == s_key ) {
			if( (*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '-' || *p == '@' ||
				*p == '_'
			) {
				/// do nothing
				continue;
			} else if ( *p == ':' ) {
				req->header_k.len = p - req->header_k.data;
				state = s_value_init;
				continue;
			} else {
				err("req headers. s_key illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_value_init ) {
			if( (*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '-' || *p == '@' ||
				*p == '_' || *p == '.' ||
				*p == '(' || *p == ')' ||
				*p == ';' || *p == '/' ||
				*p == ',' || *p == '=' ||
				*p == '*' || *p == '+' ||
				*p == ':' || *p == '"' ||
				*p == '?'
			) {
				req->header_v.data = p;
				state = s_value;
				continue;
			} else if ( *p == SP ) {
				/// do nothing 
				continue;
			} else {
				err("req headers, s_value_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_value ) {
			if( *p == SP ||
				(*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '-' || *p == '@' ||
				*p == '_' || *p == '.' ||
				*p == '(' || *p == ')' ||
				*p == ';' || *p == '/' ||
				*p == ',' || *p == '=' ||
				*p == '*' || *p == '+' ||
				*p == ':' || *p == '"' ||
				*p == '?'
			) {
				/// do nothing 
				continue;
			} else if ( *p == CR ) {
				req->header_v.len = p - req->header_v.data;
				state = s_end;
				continue;
			} else {
				err("req headers. s_value illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_end ) {
			if( *p == LF ) {
				line_end = 1;
				break;
			} else {
				err("req headers. s_end illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_done ) {
			if( *p == LF ) {
				all_end = 1;
				break;
			} else {
				err("req headers. s_done illegal [%c]\n", *p );
				return ERROR;
			}
		}
	}

	if( line_end == 1 ) {
		meta->pos = p+1;
		req->state = s_key_init;
		return OK;
	} else if ( all_end == 1 ) {
		meta->pos = p;
		if( p < meta->last ) {
			meta->pos = p+1;
		}
		req->state = 0;
		return DONE;
	} else {
		meta->pos = p;
		req->state = state;
		return AGAIN;
	}
}

static status http_req_headers( http_req_t * req )
{
	int rc = AGAIN;
	connection_t * c = req->c;
	http_req_value_t * headers;

	if( OK != mem_arr_create( &req->headers.list, sizeof(string_t) ) ) {
		err("http req headers list create failed\n");
		return ERROR;
	}
		
	while( 1 )  {
		if( rc == AGAIN )  {
			rc = http_req_recv( c, c->meta );
			if( rc < 0 ) {
				return rc;
			}
		}
		
		rc = http_req_headers_analysis( req, c->meta );
		if( rc == OK ) {
			/// line end 
	
			if( OK == http_req_find_header( &req->header_k, &headers ) ) {
				string_t * headers_obj = (string_t*)mem_arr_push( req->headers.list );
				if( !headers_obj ) {
					err("http req headers alloc headerobj form mem arr failed\n");
					return ERROR;
				}
				headers_obj->data = req->header_v.data;
				headers_obj->len = req->header_v.len;
				if( headers->handler ) {
					headers->handler( req, headers_obj, headers->offsetof );
				}
			}
			continue;
		}  else if( rc == DONE ) {
			/// all end 
			
			if( req->headers.user_agent ) {
				access_log("%.*s - %s - %.*s\n", 
					meta_len( req->req_line_start, req->req_line_end ),
					req->req_line_start,
					inet_ntoa( req->c->addr.sin_addr ),
					req->headers.user_agent->len, req->headers.user_agent->data );			
			} else {
				access_log("%.*s - %s - null\n", 
					meta_len( req->req_line_start, req->req_line_end ),
					req->req_line_start,
					inet_ntoa( req->c->addr.sin_addr ) );	
			}

			// process req body info
			req->content_type = HTTP_BODY_TYPE_NULL;
			
			if ( req->headers.transfer_encoding ) {
				req->content_type = HTTP_BODY_TYPE_CHUNK;
			} else if( req->headers.content_length ) {
				req->content_type 	= HTTP_BODY_TYPE_CONTENT;
				char num_str[64] = {0};
				
				memset( num_str, 0, sizeof(num_str) ); 
				memcpy( num_str, req->headers.content_length->data, req->headers.content_length->len );
				req->content_len = strtol( num_str, NULL, 10 );
			
				if( req->content_len <= 0 ) {
					req->content_type = HTTP_BODY_TYPE_NULL;
				}
			} 
			return DONE;
		}  else if( rc == ERROR ) {
			err("http req headers analysis failed\n");
			return ERROR;
		}
		
		if( c->meta->last == c->meta->end ) {
			err("http req headers too large");
			return ERROR;
		}
	}
}

/// fsm to analysis the http request request line 
static status http_req_request_line_analysis( http_req_t * req, meta_t * meta )
{
	unsigned char *p = NULL;
	int finish = 0;

	/// complete http request line format 
	/// method URL http_version

	/// complete URL format
	/// http://www.google.com/key?value
	/// scheme://host:port/path?query
	
	enum {
		s_method_init = 0,
		s_method,
		s_scheme_init,
		s_scheme,
		s_scheme_slash,
		s_scheme_slash_slash,
		s_host_init,
		s_host,
		s_port_init,
		s_port,
		s_uri,
		s_version_init,
		s_version,
		s_end
	} state;

	state = req->state;
	for( p = meta->pos; p < meta->last; p ++ ) {

		/// fsm use white list character check way
	
		if( state == s_method_init ) {
			if( *p == CR || *p == LF || *p == SP ) {
				continue;
			} else if ( *p >= 'A' && *p <= 'Z' ) {
				req->req_line_start = p;
				req->method.data = p;
				state = s_method;
				continue;
			} else {
				err("http req request line. s_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_method ) {
			if( *p >= 'A' && *p <= 'Z' ) {
				/// do nothing
				continue;
			} else if ( *p == SP ) {
				/// jump out of method state 
				req->method.len = p - req->method.data;
				state = s_scheme_init;
				if( req->method.len < 1 || req->method.len > 16 ) {
					err("http req request line. method string len [%d] illegal\n", req->method.len );
					return ERROR;
				}
			} else {
				err("http req request line. s_method illegal [%c]\n", *p );
				return ERROR;
			}
		}
	
		if( state == s_scheme_init ) {
			/// this state for storge scheme string data
			if( *p == SP ) {
				/// do nothing
				continue;
			} else if ( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ) {
				req->scheme.data = p;
				state = s_scheme;
				continue;
			} else if ( *p == '/' ) {
				/// if s_sheme frist character is '/', then means no scheme, is uri start
				req->uri.data = p;
				state = s_uri;
				continue;
			} else {
				err("http req request line. s_scheme_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_scheme ) {
			if( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ) {
				// do nothing
				continue;
			} else if ( *p == ':' ) {
				req->scheme.len = p - req->scheme.data;
				state = s_scheme_slash;
				continue;
			} else {
				err("http req request line. s_scheme illegal. [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_scheme_slash ) {
			if( *p == '/' ) {
				state = s_scheme_slash_slash;
				continue;
			} else {
				err("http req request line. s_scheme_slash illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_scheme_slash_slash ) {
			if( *p == '/' ) {
				state = s_host_init;
				continue;
			} else {
				err("http req request line. s_scheme_slash illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_host_init ) {
			if( (*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '.' ||
				*p == '-' ||
				*p == '_'
			) {
				req->host.data = p;
				state = s_host;
				continue;
			} else {
				err("http req request line. s_host_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_host ) {
			if( (*p >= 'A' && *p <= 'Z') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= '0' && *p <= '9') ||
				*p == '.' ||
				*p == '-' ||
				*p == '_'
			) {
				// do nothing 
				continue;
			} else if ( *p == ':' ) {
				req->host.len = p - req->host.data;
				state = s_port_init;
				continue;
			} else if ( *p == '/' ) {
				/// is s_host have '/', then means no port, is uri
				req->host.len = p - req->host.data;
				req->uri.data = p;
				state = s_uri;
				continue;
			} else {
				err("http req request line. s_host illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_port_init ) {
			if( *p >= '0' && *p <= '9' ) {
				req->port.data = p;
				state = s_port;
				continue;
			} else {
				err("http req request line. s_port_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_port ) {
			if( *p >= '0' && *p <= '9' ) {
				// donothing
				continue;
			} else if ( *p == '/' ) {
				req->port.len = p - req->port.data;
				req->uri.data = p;
				state = s_uri;
				continue;
			} else {
				err("http req request line. s_port illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_uri ) {
			if( (*p >= '0' && *p <= '9') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= 'A' && *p <= 'Z') ||
				*p == '.' || *p == '@' ||
				*p == '?' || *p == '&' ||
				*p == '=' || *p == '/' ||
				*p == '_' || *p == '-' ||
				*p == '*' || *p == '+'
			) {
				/// do nothing 
				continue;
			} else if( *p == SP ) {
				req->uri.len = p - req->uri.data;
				state = s_version_init;
				continue;
			} else {
				err("http req request line. s_uri illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_version_init ) {
			if( (*p >= '0' && *p <= '9') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= 'A' && *p <= 'Z') ||
				*p == '.' || *p == '/' 
			) {
				req->http_ver.data = p;
				state = s_version;
				continue;
			} else {
				err("http req request line, s_version_init illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_version ) {
			if( (*p >= '0' && *p <= '9') ||
				(*p >= 'a' && *p <= 'z') ||
				(*p >= 'A' && *p <= 'Z') ||
				*p == '.' || *p == '/' 
			) {
				/// do nothing 
				continue;
			} else if ( *p == CR ) {
				req->http_ver.len = p - req->http_ver.data;
				state = s_end;
                req->req_line_end = p;
				continue;
			} else {
				err("http req request line, s_version illegal [%c]\n", *p );
				return ERROR;
			}
		}

		if( state == s_end ) {
			if( *p == LF ) {
				finish = 1;
				break;
			} else {
				err("http req request line. s_end illegal [%c]\n", *p );
				return ERROR;
			}
		}

	}

	if( finish ) {
		meta->pos = p;
		if( p < meta->last ) {
			meta->pos = p + 1;
		}
		req->state = 0;
		return DONE;
	} else {
		meta->pos = p;
		req->state = state;
		return AGAIN;
	}
}


static status http_req_request_line( http_req_t * req )
{
	int32 rc = AGAIN;
	connection_t * c = req->c;

	while( 1 ) {
		/// alaways recv until error happend or parse finish 
		if( rc == AGAIN ) {
			rc = http_req_recv( c, c->meta );
			if( rc < 0 ) {
				return rc;
			}
		}
		rc = http_req_request_line_analysis( req, c->meta );
		if( rc == DONE )  {
			req->cb = http_req_headers;
			return req->cb( req );
		} else if( rc == ERROR )  {
			err("http req line analysis failed\n");
			return ERROR;
		}
		
		/// request size too much 
		if( c->meta->last == c->meta->end )  {
			err("http req line too long\n");
			return ERROR;
		}
	}
}

static status http_req_alloc( http_req_t ** req )
{
	http_req_t * req_n;
	queue_t * q;

	if( queue_empty( &g_queue_usable ) == 1 ) {
		err("http req g_queue_usable empty\n");
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert_tail( &g_queue_use, q );
	req_n = ptr_get_struct( q, http_req_t, queue );

	*req = req_n;
	return OK;
}

inline static void http_req_clear_string( string_t * str)
{
	str->len 	= 0;
	str->data 	= NULL;
}

status http_req_free( http_req_t * req )
{
	req->c = NULL;
	req->cb = NULL;
	req->state = 0;

	req->req_line_start = NULL;
	req->req_line_end 	= NULL;
	
	http_req_clear_string( &req->method );
	http_req_clear_string( &req->scheme );
	http_req_clear_string( &req->host );
	http_req_clear_string( &req->uri );
	http_req_clear_string( &req->port );
	http_req_clear_string( &req->http_ver );
	
	http_req_clear_string( &req->header_k );
	http_req_clear_string( &req->header_v );
	
	if( req->headers.list ) {
		mem_arr_free( req->headers.list );
        memset( &req->headers, 0, sizeof(http_req_headers_t) );
	}
	req->keepalive = 0;
	req->content_len = 0;
	req->content_type = 0;
	
	queue_remove( &req->queue );
	queue_insert_tail( &g_queue_usable, &req->queue );
	return OK;
}


status http_req_create( connection_t * c, http_req_t ** request )
{
	/*
		http request 

		<request line>	/// this module process
		<headers>		/// this module process
		<request body>	/// process by http body module
	*/
	http_req_t * req_n = NULL;

	if( OK != http_req_alloc(&req_n) ) {
		err("http req alloc new failed\n");
		return ERROR;
	}
	req_n->c = c;
	req_n->state = 0;
	req_n->cb = http_req_request_line;
	
	*request = req_n;
	return OK;
}

status http_req_init_module( void )
{
	int i;

	queue_init( &g_queue_use );
	queue_init( &g_queue_usable );
	g_pool = (http_req_t*)l_safe_malloc( sizeof(http_req_t)*MAX_NET_CON );
	if( !g_pool )  {
		err("http req header module. alloc g_pool failed. [%d]\n", errno );
		return ERROR;
	}

	for( i = 0; i < MAX_NET_CON; i++ )  {
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}
	return OK;
}

status http_req_end_module( void )
{
	if( g_pool ) {
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}

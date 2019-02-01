#include "lk.h"

static queue_t 	in_use;
static queue_t 	usable;
http_request_head_t * pool = NULL;


static status http_request_head_set_value( http_request_head_t * request,
	string_t * value,
	uint32 offsetof );
static status http_request_head_set_connection( http_request_head_t * request,
	string_t * value,
	uint32 offsetof );

static struct header_t headers[] = {
	{ 	string("Host"),				http_request_head_set_value,offsetof(request_headers_in_t, host) },
	{ 	string("Content-Length"),	http_request_head_set_value,offsetof(request_headers_in_t, content_length) },
	{ 	string("Connection"),		http_request_head_set_connection,offsetof(request_headers_in_t,	connection)	},
	{ 	string("Transfer-Encoding"),http_request_head_set_value,offsetof(request_headers_in_t,	transfer_encoding)	},
	{   string("User-Agent"),		http_request_head_set_value,offsetof(request_headers_in_t,	user_agent) }
};

// http_request_head_set_value -------------
static status http_request_head_set_value( http_request_head_t * request,
	string_t * value,
	uint32 offsetof )
{
	string_t ** h;
	h = (string_t**) ((char*) &request->headers_in + offsetof );
	*h = value;
	return OK;
}
// http_request_head_set_connection ---------------
static status http_request_head_set_connection( http_request_head_t * request,
	string_t * value,
	uint32 offsetof )
{
	string_t ** h;
	h = (string_t**) ((char*) &request->headers_in + offsetof );
	*h = value;

	request->keepalive_flag = (value->len > (int32)l_strlen("close")) ? 1 :0;
	return OK;
}
// http_request_head_find_header -----------------------------
static status http_request_head_find_header( string_t * str,
	struct header_t ** header )
{
	int32 i = 0;
	for( i = 0; i < (int32)(sizeof(headers)/sizeof(struct header_t));
	 i ++ ) {
		if( headers[i].name.len == str->len ) {
			if( OK == l_strncmp_cap( headers[i].name.data, headers[i].name.len, str->data, str->len ) ) {
				*header = &headers[i];
				return OK;
			}
		}
	}
	return ERROR;
}
// http_request_head_recv ------
static status http_request_head_recv( connection_t * c, meta_t * meta )
{
	ssize_t rc = 0;

	if( meta_len( meta->pos, meta->last ) > 0 ) {
		return OK;
	}
	rc = c->recv( c, meta->last,
		meta_len( meta->last, meta->end ) );
	if( rc == ERROR ) {
		return ERROR;
	} else if ( rc == AGAIN ) {
		debug_log("%s --- again", __func__ );
		return AGAIN;
	} else {
		meta->last += rc;
		return OK;
	}
}
// http_request_head_parse_request_line --------------------------------
static status http_request_head_parse_request_line( http_request_head_t * request, meta_t * meta )
{
	char *p = NULL;

	enum {
		s_start = 0,
		s_method,
		s_after_method,
		s_after_connect,
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
	for( p = meta->pos; p < meta->last; p ++ ) {
		switch( state ) {
			case	s_start:
				if(
				( *p >= 'A' && *p <= 'Z' ) ||
				*p == '\r' ||
				*p == '\n' ||
				*p == ' ') {
					if(
					*p == '\r' ||
					*p == '\n' ||
					*p == ' ' ) {
						break;
					}
					request->method.data = p;
					state = s_method;
				}
				break;
			case	s_method:
				if(
				( *p >= 'A' && *p <= 'Z' ) ||
				*p == ' ' ) {
					if( *p == ' ' ) {
						request->method.len = meta_len( request->method.data, p );
						if( request->method.len > 8 ) {
							err_log( "%s --- method length more than 8", __func__ );
							return ERROR;
						} else if ( request->method.len < 1  ) {
							err_log( "%s --- method length less than 1", __func__ );
							return ERROR;
						}
						if( OK == l_strncmp_cap( request->method.data, request->method.len, "CONNECT", l_strlen("CONNECT") ) ) {
							state = s_after_connect;
							break;
						}
						state = s_after_method;
					}
				} else {
					err_log( "%s --- method illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_after_method:
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				*p == '/' ||
				*p == ' ' ) {
					if( *p >= 'a' && *p <= 'z' ) {
						request->schema.data = p;
						state = s_schema;
					} else if ( *p == '/' ) {
						request->uri.data = p;
						state = s_uri;
					}
				} else {
					err_log( "%s --- after method illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case	 s_after_connect:
				if( *p == ' ' ){
					break;
				} else if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= '0' && *p <= '9' )
				) {
					request->host.data = p;
					state = s_host;
				} else {
					err_log( "%s --- after connect illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;

			case	s_schema:
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				*p == ':' ) {
					if(  *p == ':' ){
						request->schema.len = meta_len( request->schema.data, p );
						state = s_schema_slash;
					}
				} else {
					err_log( "%s --- schema error [%c]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_schema_slash:
				if( *p == '/' ) {
					state = s_schema_slash_slash;
				} else {
					err_log( "%s --- schema slash not '//'", __func__ );
				}
				break;
			case	s_schema_slash_slash:
				if( *p == '/' ) {
					state = s_host_start;
				} else {
					err_log( "%s --- schema slash slash not '//'", __func__ );
				}
				break;
			case	s_host_start:
				request->host.data = p;
				state = s_host;
				break;
			case	s_host:
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= 'A' && *p <= 'Z' ) ||
				( *p >= '0' && *p <= '9' ) ||
				*p == '.' ||
				*p == '/' ||
				*p == ':' ||
				*p == '-' ||
				*p == '_' ) {
					if( *p == '/' ) {
						request->host.len = meta_len( request->host.data, p );
						request->uri.data = p;
						state = s_uri;
					} else if ( *p == ':' ) {
						request->host.len = meta_len( request->host.data, p );
						state = s_colon;
					}
				} else {
					err_log( "%s --- host illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case s_colon:
				if( *p >= '0' && *p <= '9' ) {
					request->port.data = p;
					state = s_port;
				} else {
					err_log( "%s --- colon illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case  s_port:
				if(
				(*p >= '0' && *p <= '9' ) ||
				*p == ' ' ||
				*p == '/' ) {
					if( *p == ' ' ) {
						request->port.len = meta_len( request->port.data, p );
						state = s_after_uri;
					} else if ( *p == '/' ) {
						request->port.len = meta_len( request->port.data, p );
						request->uri.data = p;
						state = s_uri;
					}
				} else {
					err_log( "%s --- port illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_uri:

				if( *p == ' ' ) {
					request->uri.len = meta_len( request->uri.data, p );
					state = s_after_uri;
				}
				break;
			case	s_after_uri:
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= 'A' && *p <= 'Z' ) ||
				*p == ' ' ) {
					if(
					(*p >= 'a' && *p <= 'z') ||
					( *p >= 'A' && *p <= 'Z' )) {
						request->http_version.data = p;
						state = s_version;
					}
				} else {
					err_log( "%s --- uri illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_version:
				if( p - request->http_version.data > 8 ) {
					err_log( "%s --- http version more than 8", __func__ );
					return ERROR;
				}
				if(
				( *p >= 'a' && *p <= 'z' ) ||
				( *p >= 'A' && *p <= 'Z' ) ||
				( *p >= '0' && *p <= '9' ) ||
				*p == '/' ||
				*p == '.' ||
				*p == '\r' ) {
					if( *p == (u_char)'\r' ) {
						request->request_line_end = p;
						request->http_version.len = meta_len( request->http_version.data, p );
						state = s_end;
					}
				}  else {
					err_log( "%s --- http version illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_end:
				if( *p != (u_char)'\n' ) {
					err_log( "%s --- end illegal [%d]", __func__, *p );
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
	request->state = s_start;
	return DONE;
}
// http_request_head_parse_header_line -----
static status http_request_head_parse_header_line( http_request_head_t * request, meta_t * meta )
{
	char *p = NULL;

	enum {
		s_start = 0,
		s_name,
		s_name_after,
		s_value,
		s_almost_done,
		s_header_end_r
	} state;

	state = request->state;
	for( p = meta->pos; p < meta->last; p ++ ) {

		switch( state ) {
			case	s_start:
				request->header_key.data = p;
				if(
				( *p >= 32 && *p <= 126 ) ||
				*p == '\r'  ) {
					if( *p == '\r' ) {
						state = s_header_end_r;
						break;
					}
					state = s_name;
					break;
				} else {
					err_log( "%s --- start not support [%d]", __func__ , *p );
					return ERROR;
				}
			case	s_name:
				if(
				( *p >= 32 && *p <= 126 )
				 ) {
					if( *p == ':' ) {
						request->header_key.len = meta_len( request->header_key.data, p );
						state = s_name_after;
						break;
					}
				} else {
					err_log ( "%s --- s_name not support [%c]", __func__, *p );
					return ERROR;
				}
				break;
			case	s_name_after:

				if( *p != ' ' ) {
					request->header_value.data = p;
					state = s_value;
				}
				break;
			case	s_value:
				if( *p == '\r' ) {
					request->header_value.len = meta_len( request->header_value.data, p );
					state = s_almost_done;
				}
				break;
			case	s_almost_done:
				if( *p != '\n' ) {
					err_log( "%s --- s_almost_done not \n", __func__ );
					return ERROR;
				}
				goto  done;
			case 	s_header_end_r:
				if( *p != '\n' ) {
					err_log( "%s --- s_header_end_r not \n", __func__ );
					return ERROR;
				}
				goto  header_done;
		}
	}
	meta->pos = p;
	request->state = state;
	return AGAIN;
done:
	meta->pos = p+1;
	request->state = s_start;
	return OK;
header_done:
	meta->pos = p+1;
	request->state = s_start;
	return HTTP_PARSE_HEADER_DONE;
}
// http_request_head_process_headers ----------------------------------------
static status http_request_head_process_headers( http_request_head_t * request )
{
	int rc = AGAIN;
	connection_t * c;
	struct header_t * headers;
	string_t  *str_name, *str_value;
	int32 content_length;
	string_t user_agent = string("empty");

	c = request->c;
	if( OK != mem_list_create( &request->headers_in.list,
		sizeof(string_t) ) ) {
		err_log ( "%s --- headers_in list create", __func__ );
		return ERROR;
	}
	while( 1 ) {
		if( rc == AGAIN ) {
			rc = http_request_head_recv( c, c->meta );
			if( rc == AGAIN || rc == ERROR ) {
				if( rc == ERROR ) {
					err_log( "%s --- read headers", __func__ );
				}
				return rc;
			}
		}
		rc = http_request_head_parse_header_line( request, c->meta );
		if( rc == OK ) {
			str_name = &request->header_key;
			str_value = (string_t*)mem_list_push( request->headers_in.list );
			str_value->data = request->header_value.data;
			str_value->len = request->header_value.len;
			if( OK == http_request_head_find_header( str_name, &headers ) ) {
				if( headers->handler ) {
					headers->handler( request, str_value, headers->offsetof );
				}
			}
			continue;
		} else if( rc == HTTP_PARSE_HEADER_DONE ) {
			if( request->headers_in.user_agent ) {
				user_agent.len = request->headers_in.user_agent->len;
				user_agent.data = request->headers_in.user_agent->data;
			}
			access_log("%.*s - %s - %.*s",
			meta_len( request->request_line_start, request->request_line_end ), request->request_line_start,
			inet_ntoa( request->c->addr.sin_addr ),
			user_agent.len, user_agent.data );

			if( request->headers_in.content_length ) {
				request->body_type = HTTP_ENTITYBODY_CONTENT;
				if( OK != l_atoi( request->headers_in.content_length->data,
				request->headers_in.content_length->len,
			 	&content_length ) ) {
					err_log("%s --- l_atoi content length", __func__ );
					return ERROR;
				}
				request->content_length = (uint32)content_length;
			} else if ( request->headers_in.transfer_encoding ) {
				request->body_type = HTTP_ENTITYBODY_CHUNK;
			} else {
				request->body_type = HTTP_ENTITYBODY_NULL;
			}
			return DONE;
		} else if( rc == ERROR ) {
			err_log( "%s --- parse header line", __func__ );
			return ERROR;
		}
		if( c->meta->last == c->meta->end ) {
			err_log( "%s --- headers too large", __func__ );
			return ERROR;
		}
	}
}
// http_request_head_process_request_line ------
static status http_request_head_process_request_line( http_request_head_t * request )
{
	int32 rc = AGAIN;
	connection_t * c;

	c = request->c;
	while( 1 ) {
		if( rc == AGAIN ) {
			rc = http_request_head_recv( c, c->meta );
			if( rc == AGAIN || rc == ERROR ) {
				if( rc == ERROR ) {
					err_log( "%s --- read header", __func__ );
				}
				return rc;
			}
		}
		rc = http_request_head_parse_request_line( request, c->meta );
		if( rc == DONE ) {
			if( request->uri.len > REQ_LENGTH_URI_STR ) {
				err_log("%s --- uri too long", __func__ );
				return ERROR;
			}
			request->request_line_start = c->meta->start;
			request->handler = http_request_head_process_headers;
			return request->handler( request );
		} else if( rc == ERROR ) {
			err_log( "%s --- request parse line", __func__ );
			return ERROR;
		}
		// request header size muse less than meta's space
		if( c->meta->last == c->meta->end ) {
			err_log( "%s --- request line too long", __func__ );
			return ERROR;
		}
	}
}

// http_request_head_alloc----
static status http_request_head_alloc( http_request_head_t ** request )
{
	http_request_head_t * new;
	queue_t * q;

	if( queue_empty( &usable ) ) {
		err_log("%s --- usable empty", __func__ );
		return ERROR;
	}
	q = queue_head( &usable );
	queue_remove( q );
	queue_insert_tail( &in_use, q );
	new = l_get_struct( q, http_request_head_t, queue );

	*request = new;
	return OK;
}
// http_request_head_create -------
status http_request_head_create( connection_t * c, http_request_head_t ** request )
{
	http_request_head_t * new;

	if( OK != http_request_head_alloc(&new) ) {
		err_log("%s --- request alloc", __func__ );
		return ERROR;
	}
	new->handler = http_request_head_process_request_line;
	new->c = c;
	new->state = 0;
	*request = new;
	return OK;
}
// http_request_head_free ------
status http_request_head_free( http_request_head_t * request )
{
	queue_remove( &request->queue );
	queue_insert_tail( &usable, &request->queue );

	request->c = NULL;
	request->handler = NULL;
	request->state = 0;

	request->request_line_start = NULL;
	request->request_line_end = NULL;
	request->method.data = NULL;
	request->method.len = 0;
	request->schema.data = NULL;
	request->schema.len = 0;
	request->host.data = NULL;
	request->host.len = 0;
	request->uri.data = NULL;
	request->uri.len = 0;
	request->port.data = NULL;
	request->port.len = 0;
	request->http_version.data = NULL;
	request->http_version.len = 0;

	request->header_key.data = NULL;
	request->header_key.len = 0;
	request->header_value.data = NULL;
	request->header_value.len = 0;

	if( request->headers_in.list ) {
		mem_list_free( request->headers_in.list );
		memset( &request->headers_in, 0, sizeof(request_headers_in_t) );
	}
	request->keepalive_flag = 0;
	request->body_type = 0;
	request->content_length = 0;

	return OK;
}
// http_request_head_init_module ---
status http_request_head_init_module( void )
{
	uint32 i;

	queue_init( &in_use );
	queue_init( &usable );
	pool = (http_request_head_t*)l_safe_malloc( sizeof(http_request_head_t)*MAXCON );
	if( !pool ) {
		err_log("%s --- request pool", __func__ );
		return ERROR;
	}
	memset( pool, 0, sizeof(http_request_head_t)*MAXCON );
	for( i = 0; i < MAXCON; i++ ) {
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}
// http_request_head_end_module ----
status http_request_head_end_module( void )
{
	if( pool ) {
		l_safe_free( pool );
		pool = NULL;
	}
	return OK;
}

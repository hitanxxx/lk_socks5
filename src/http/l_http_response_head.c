#include "lk.h"

static queue_t	usable;
static queue_t 	in_use;
static http_response_head_t * pool = NULL;

// http_response_head_recv ----------------------
static status http_response_head_recv( http_response_head_t * response  )
{
	ssize_t rc = 0;

	if( response->c->meta->last - response->c->meta->pos > 0 ) {
		return OK;
	}
	rc = response->c->recv( response->c, response->c->meta->last, meta_len( response->c->meta->last, response->c->meta->end ) );
	if( rc == ERROR ) {
		return ERROR;
	} else if ( rc == AGAIN ) {
		return AGAIN;
	} else {
		response->c->meta->last += rc;
	}
	return OK;
}

// http_response_head_parse_header_line -------------------------------------
static status http_response_head_parse_header_line( http_response_head_t * response, meta_t * meta )
{
	char *p = NULL;

	enum {
		s_start = 0,
		s_name,
		s_value_before,
		s_value,
		s_almost_done,
		s_header_end_r
	} state;

	state = response->state;
	for( p = (char*)meta->pos; p < (char*)meta->last; p ++ ) {
		switch( state ) {
			case s_start:
				if(
				( *p >= 32 && *p <= 126 ) ||
				*p == '\r' ) {
					if( *p == '\r' ) {
						state = s_header_end_r;
						break;
					}
					response->header_key.data = p;
					state = s_name;
					break;
				} else {
					err_log(  "%s --- start illegal [%d]", __func__ , *p );
					return ERROR;
				}
			case s_name:
				if(
				( *p >= 32 && *p <= 126 )
				) {
					if( *p == ':' ) {
						response->header_key.len = meta_len( response->header_key.data, p );
						state = s_value_before;
						break;
					}
				} else {
					debug_log(  "%s --- s_name illegal [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case s_value_before:
				if( *p != ' ' ) {
					response->header_value.data = p;
					state = s_value;
				}
				break;
			case s_value:
				if( *p == '\r' ) {
					response->header_value.len = meta_len( response->header_value.data, p );
					state = s_almost_done;
				}
				break;
			case s_almost_done:
				if( *p != '\n' ) {
					err_log(  "%s --- s_almost_done illegal [%d]", __func__, *p );
					return ERROR;
				}
				goto done;
				break;
			case s_header_end_r:
				if( *p != '\n' ) {
					err_log(  "%s --- s_header_end_r illegal [%d]", __func__, *p );
					return ERROR;
				}
				goto  header_done;
				break;
		}
	}
	meta->pos = p;
	response->state = state;
	return AGAIN;
done:
	meta->pos = p+1;
	response->state = s_start;
	return OK;
header_done:
	meta->pos = p+1;
	response->state = s_start;
	return HTTP_PARSE_HEADER_DONE;
}

// http_response_head_parse_response_line ------------------
static int32 http_response_head_parse_response_line( http_response_head_t * response, meta_t * meta )
{
	char * p = NULL;

	enum {
		start,
		h,
		ht,
		htt,
		http,
		http_slash,
		version,
		code_start,
		code,
		before_string,
		string,
		r,
	} state;

	state = response->state;
	for( p=(char*)meta->pos; p < (char*)meta->last; p++ ) {

		switch( state ) {
			case start:
				if( *p == 'H' || *p == 'h' ) {
					state = h;
				} else {
					err_log(  "%s --- start illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case h:
				if( *p == 'T' || *p == 't' ) {
					state = ht;
				} else {
					err_log( "%s --- h illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case ht:
				if( *p == 'T' || *p == 't' ) {
					state = htt;
				} else {
					err_log("%s --- ht illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case htt:
				if( *p == 'P' || *p == 'p' ) {
					state = http;
				} else {
					err_log("%s --- htt illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case http:
				if( *p == '/' ) {
					state = http_slash;
				} else {
					err_log("%s --- http illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case http_slash:
				if( (*p <= '9' && *p >= '0' ) ||
				*p == '.' ) {
					response->http_version.data = p;
					state = version;
				} else {
					err_log("%s --- http_slash illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case version:
				if( ( *p <= '9' && *p >= '0' ) ||
				*p == '.' ||
				*p == ' ' ) {
					if( *p == ' ' ) {
						response->http_version.len = meta_len( response->http_version.data, p );
						if( response->http_version.len < 1 ) {
							err_log(  "%s --- http_version length < 1", __func__ );
							return ERROR;
						} else {
							state = code_start;
						}
						break;
					}
				} else {
					err_log(  "%s --- version illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case code_start:
				if( *p <= '9' && *p >= '0' ) {
					response->http_code.data = p;
					state = code;
				} else {
					err_log(  "%s --- before code illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case code:
				if( ( *p <= '9' && *p >= '0' ) ||
				*p == ' ' ) {
					if( *p == ' ' ) {
						response->http_code.len = meta_len( response->http_code.data, p );
						if( response->http_code.len != 3 ) {
							err_log(  "%s --- http_code length != 3, [%d]", __func__, response->http_code.len );
							return ERROR;
						}
						state = before_string;
						break;
					}
				} else {
					err_log(  "%s --- code illegal, [%d]", __func__, *p );
					return ERROR;
				}
				break;
			case before_string:
				response->http_string.data = p;
				state = string;
				break;
			case string:
				if( *p == '\r' ) {
					response->http_string.len = meta_len( response->http_string.data, p );
					if( response->http_string.len < 1 ) {
						err_log(  "%s --- http_string length < 1, illegal", __func__ );
						return ERROR;
					}
					state = r;
				}
				break;
			case r:
				if( *p != '\n' ) {
					err_log(  "%s --- r illegal [%d]", __func__, *p );
					return ERROR;
				}
				goto done;
				break;
			default:
				break;
		}
	}
	meta->pos = p+1;
	response->state = state;
	return AGAIN;
done:
	meta->pos = p+1;
	return DONE;
}
// http_response_head_process_headers -------------
static status http_response_head_process_headers( http_response_head_t * response )
{
	int32 rc = AGAIN;
	string_t * key, * value;
	int32 length;
	string_t content_str_length = string("Content-Length");
	string_t transfer_str_length = string("Transfer-Encoding");
	string_t connection_str_length = string("Connection");

	while(1) {
		if( rc == AGAIN ) {
			rc = http_response_head_recv( response );
			if( rc == AGAIN || rc == ERROR ) {
				if( rc == ERROR ) {
					err_log(  "%s --- http_response_head_recv error", __func__ );
				}
				return rc;
			}
		}
		rc = http_response_head_parse_header_line( response, response->c->meta );
		if( rc == OK ) {
			key = &response->header_key;
			value = &response->header_value;
			if( OK == l_strncmp_cap( key->data, key->len, connection_str_length.data, connection_str_length.len ) ) {
				if( value->len > l_strlen("close") ) {
					response->keepalive = 1;
				} else {
					response->keepalive = 0;
				}
			} else if( OK == l_strncmp_cap( key->data, key->len, content_str_length.data, content_str_length.len ) ) {
				response->body_type = HTTP_ENTITYBODY_CONTENT;
				if( OK != l_atoi( value->data, value->len, &length ) ){
					err_log( "%s --- l_atoi content length", __func__ );
					return ERROR;
				}
				response->content_length = (uint32)length;
			} else if( OK == l_strncmp_cap( key->data, key->len, transfer_str_length.data, transfer_str_length.len ) ) {
				response->body_type = HTTP_ENTITYBODY_CHUNK;
			}
			continue;
		} else  if( rc == HTTP_PARSE_HEADER_DONE ) {
			debug_log(  "%s --- success", __func__ );
			response->head.start = response->head.pos = response->c->meta->start;
			response->head.last = response->head.end = response->c->meta->pos;
			return DONE;
		} else if ( rc == ERROR ) {
			err_log(  "%s --- parse headers error", __func__ );
			return ERROR;
		}
		if( meta_len( response->c->meta->last, response->c->meta->end ) < 1 ) {
			err_log(  "%s --- header too lager", __func__ );
			return ERROR;
		}
	}
}
// http_response_head_process_response_line ---------------------
static status http_response_head_process_response_line( http_response_head_t * response )
{
	int32 rc = AGAIN;
	int32 http_code;

	while(1) {
		if( rc == AGAIN ) {
			rc = http_response_head_recv( response );
			if( rc == AGAIN || rc == ERROR ) {
				if( rc == ERROR ) {
					err_log(  "%s --- http_response_head_recv error", __func__, rc );
				}
				return rc;
			}
		}
		rc = http_response_head_parse_response_line( response, response->c->meta );
		if( rc == DONE ) {
			if( OK != l_atoi( response->http_code.data,
				response->http_code.len, &http_code ) )
			{
				err_log( "%s --- l_atoi http code", __func__ );
				return ERROR;
			}
			response->http_status_code = (uint32)http_code;
			response->handler = http_response_head_process_headers;
			return response->handler( response );

		} else if ( rc == ERROR ) {
			err_log(  "%s --- parse status line error", __func__ );
			return ERROR;
		}
		if( meta_len( response->c->meta->last, response->c->meta->end ) < 1 ) {
			err_log(  "%s --- header too large", __func__ );
			return ERROR;
		}
	}
}
// http_response_head_alloc ----------------------
static status http_response_head_alloc( http_response_head_t ** response )
{
	http_response_head_t * new;
	queue_t * q;

	if( queue_empty( &usable ) == 1 ) {
		err_log(  "%s --- response queue empty", __func__ );
		return ERROR;
	}
	q = queue_head( &usable );
	new = l_get_struct( q, http_response_head_t, queue );
	queue_remove( q );
	queue_insert_tail( &in_use, q );
	*response = new;
	return OK;
}

// http_response_head_free ---------------------
status http_response_head_free( http_response_head_t * response )
{
	queue_remove( &response->queue );
	queue_insert_tail( &usable, &response->queue );

	response->state = 0;
	response->c = NULL;
	response->handler = NULL;

	response->header_key.data = NULL;
	response->header_key.len = 0;
	response->header_value.data = NULL;
	response->header_value.len = 0;

	response->http_code.data = NULL;
	response->http_code.len = 0;
	response->http_version.data = NULL;
	response->http_version.len = 0;
	response->http_string.data = NULL;
	response->http_string.len = 0;
	response->http_status_code = 0;

	response->body_type = HTTP_ENTITYBODY_NULL;
	response->content_length = 0;
	response->keepalive = 0;

	response->head.pos = response->head.last = response->head.start;
	return OK;
}

// http_response_head_create -----------------------------------
status http_response_head_create( connection_t * c, http_response_head_t ** response )
{
	http_response_head_t * new = NULL;

	if( OK != http_response_head_alloc( &new ) ) {
		err_log(  "%s --- response alloc", __func__ );
		return ERROR;
	}
	new->handler = http_response_head_process_response_line;
	new->state = 0;
	new->c = c;

	*response = new;
	return OK;
}

// http_response_head_init_module -------------
status http_response_head_init_module ( void )
{
	uint32 i;

	queue_init( &in_use );
	queue_init( &usable );

	pool = (http_response_head_t*)l_safe_malloc( sizeof(http_response_head_t)* MAXCON );
	if( !pool ) {
		err_log(  "%s --- l_safe_malloc pool", __func__ );
		return ERROR;
	}
	memset( pool, 0, sizeof(http_response_head_t)*MAXCON );
	for( i = 0; i < MAXCON; i ++ ) {
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}

// http_response_head_end_module ----------
status http_response_head_end_module( void )
{
	if( pool ) {
		l_safe_free( pool );
	}
	return OK;
}

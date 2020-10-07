#include "l_base.h"
#include "l_http_response_head.h"

static queue_t	usable;
static queue_t 	in_use;
static http_response_head_t * pool = NULL;


static status http_response_head_recv( http_response_head_t * response  )
{
	ssize_t rc = 0;

	if( response->c->meta->last - response->c->meta->pos > 0 ) 
	{
		return OK;
	}
	rc = response->c->recv( response->c, response->c->meta->last, meta_len( response->c->meta->last, response->c->meta->end ) );
	if( rc < 0 )
	{
        if( ERROR == rc ) return ERROR;
        return AGAIN;
	}
	response->c->meta->last += rc;
	return OK;
}

static status http_response_head_parse_header_line( http_response_head_t * response, meta_t * meta )
{
	unsigned char *p = NULL;

	enum {
		s_start = 0,
		s_name,
		s_value_before,
		s_value,
		s_header_end_r,
		s_headers_end_r
	} state;

	state = response->state;
	for( p = meta->pos; p < meta->last; p ++ )
	{
		switch( state ) 
		{
			case s_start:
				if( ( *p >= 32 && *p <= 126 ) ||*p == '\r' )
				{
					if( *p == '\r' ) 
					{
						state = s_headers_end_r;
					}
					else
					{
						response->header_key.data = p;
						state = s_name;
					}
				} 
				else 
				{
					err("http resp header start illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_name:
				if(( *p >= 32 && *p <= 126 ))
				{
					if( *p == ':' ) 
					{
						response->header_key.len = meta_len( response->header_key.data, p );
						state = s_value_before;
						break;
					}
				} 
				else 
				{
					debug("http resp header s_name illegal [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_value_before:
				if( *p != ' ' )
				{
					response->header_value.data = p;
					state = s_value;
				}
			case s_value:
				if( *p == '\r' ) 
				{
					response->header_value.len = meta_len( response->header_value.data, p );
					state = s_header_end_r;
				}
				break;
			case s_header_end_r:
				if( *p != '\n' ) 
				{
					err("http resp header s_header_end illegal [%d]\n", *p );
					return ERROR;
				}
				goto done;
				break;
			case s_headers_end_r:
				if( *p != '\n' )
				{
					err("http resp header s_headers_end_r illegal [%d]\n", *p );
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
	return DONE;
}

static int32 http_response_head_parse_response_line( http_response_head_t * response, meta_t * meta )
{
	unsigned char * p = NULL;

	enum {
		start,
		s_h,
		s_ht,
		s_htt,
		s_http,
		s_http_slash,
		s_version,
		s_code_start,
		s_code,
		s_before_string,
		s_string,
		s_line_end_r,
	} state;

	//  http_version http_status_code http_string
	
	state = response->state;
	for( p= meta->pos; p < meta->last; p++ )
	{
		switch( state ) 
		{
			case start:
				if( *p == 'H' || *p == 'h' ) 
				{
					state = s_h;
				} 
				else 
				{
					err("http resp header s_start illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_h:
				if( *p == 'T' || *p == 't' ) 
				{
					state = s_ht;
				} 
				else 
				{
					err("http resp header s_t illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_ht:
				if( *p == 'T' || *p == 't' )
				{
					state = s_htt;
				} 
				else 
				{
					err("http resp header s_ht illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_htt:
				if( *p == 'P' || *p == 'p' ) 
				{
					state = s_http;
				} 
				else 
				{
					err("http resp header htt illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_http:
				if( *p == '/' ) 
				{
					state = s_http_slash;
				} 
				else 
				{
					err("http resp header s_http illegal, [%d]\n",*p );
					return ERROR;
				}
				break;
			case s_http_slash:
				if( (*p <= '9' && *p >= '0' ) || *p == '.' )
				{
					response->http_version.data = p;
					state = s_version;
				}
				else 
				{
					err("http resp header s_http_slash illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_version:
				if( ( *p <= '9' && *p >= '0' ) ||*p == '.' || *p == ' ' ) 
				{
					if( *p == ' ' ) 
					{
						response->http_version.len = meta_len( response->http_version.data, p );
						state = s_code_start;
					}
				} 
				else 
				{
					err("http resp header s_version illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_code_start:
				if( *p >= '0' && *p <= '9' ) 
				{
					response->http_code.data = p;
					state = s_code;
				} 
				else 
				{
					err("http resp header s_code_start illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_code:
				if( ( *p <= '9' && *p >= '0' ) || *p == ' ' ) 
				{
					if( *p == ' ' ) 
					{
						response->http_code.len = meta_len( response->http_code.data, p );
						state = s_before_string;
					}
				}
				else 
				{
					err("code illegal, [%d]\n", *p );
					return ERROR;
				}
				break;
			case s_before_string:
				response->http_string.data = p;
				state = s_string;
			case s_string:
				if( *p == '\r' ) 
				{
					response->http_string.len = meta_len( response->http_string.data, p );
					state = s_line_end_r;
				}
				break;
			case s_line_end_r:
				if( *p != '\n' )
				{
					err("http resp header s_line_end_r illegal [%d]\n", *p );
					return ERROR;
				}
				goto done;
				break;
			default:
				break;
		}
	}
	meta->pos = (unsigned char*)(p+1);
	response->state = state;
	return AGAIN;
done:
	meta->pos = (unsigned char*)(p+1);
	return DONE;
}

static status http_response_head_process_headers( http_response_head_t * response )
{
	int32 rc = AGAIN;
	string_t * key = NULL, * value = NULL;
	string_t content_str 		= string("Content-Length");
	string_t transfer_str 		= string("Transfer-Encoding");
	string_t connection_str 	= string("Connection");

	while(1) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_response_head_recv( response );
			if( rc < 0 ) 
			{
				if( rc == ERROR ) 
				{
					err("http resp header line recv failed\n");
				}
				return rc;
			}
		}
		rc = http_response_head_parse_header_line( response, response->c->meta );
		if ( rc == ERROR )
		{
			err("http resp head parse header line failed\n");
			return ERROR;
		}
		else if( rc == OK )
		{
			key 	= &response->header_key;
			value 	= &response->header_value;
			if( OK == l_strncmp_cap( key->data, key->len, connection_str.data, connection_str.len ) ) 
			{
				if( value->len > l_strlen("close") ) 
				{
					response->keepalive = 1;
				} 
			}
			else if( OK == l_strncmp_cap( key->data, key->len, content_str.data, content_str.len ) ) 
			{
				if( OK != l_atoi( value->data, value->len, ((int32*)&response->content_length) ) )
				{
					err("http resp head conv content length failed, [%.*s]\n", value->len, value->data );
					return ERROR;
				}
				response->body_type = HTTP_BODY_TYPE_CONTENT;
			} 
			else if( OK == l_strncmp_cap( key->data, key->len, transfer_str.data, transfer_str.len ) ) 
			{
				response->body_type = HTTP_BODY_TYPE_CHUNK;
			}
			continue;
		}
		else if( rc == DONE ) 
		{
			// meta head for caller
			response->head.start 	= response->head.pos = response->c->meta->start;
			response->head.last 	= response->head.end = response->c->meta->pos;
			return DONE;
		}
		
		if( meta_len( response->c->meta->last, response->c->meta->end ) < 1 )
		{
			err("http resp header line too long\n");
			return ERROR;
		}
	}
}

static status http_response_head_process_response_line( http_response_head_t * response )
{
	int32 rc = AGAIN;
	int32 http_code;

	while(1) 
	{
		if( rc == AGAIN ) 
		{
			rc = http_response_head_recv( response );
			if( rc < 0 )
			{
				if( rc == ERROR ) 
				{
					err("http resp header recv failed\n", rc );
				}
				return rc;
			}
		}
		rc = http_response_head_parse_response_line( response, response->c->meta );
		if( rc == ERROR )
		{
			err( " parse status line error\n");
			return ERROR;
		}
		else if( rc == DONE ) 
		{
			// check response line string length frist
			if( response->http_version.len < 8 ) // HTTP/X.X
			{
				err("http resp head http_version len < 8\n");
				return ERROR;
			}
			if( response->http_status_code <= 0 || response->http_status_code%1000 > 0 )
			{
				err("http resp head status_code illegal, [%d]\n", response->http_status_code );
				return ERROR;
			}
			
			if( OK != l_atoi( response->http_code.data, response->http_code.len, &http_code ) )
			{
				err("http resp header conv http_code failed, [%.*s]\n", response->http_code.len, response->http_code.data );
				return ERROR;
			}
			response->handler.process 	= http_response_head_process_headers;
			response->http_status_code 	= (uint32)http_code;
			return response->handler.process( response );
		} 
		
		if( meta_len( response->c->meta->last, response->c->meta->end ) < 1 ) 
		{
			err("http resp header too long\n");
			return ERROR;
		}
	}
}

static status http_response_head_alloc( http_response_head_t ** response )
{
	http_response_head_t * new;
	queue_t * q;

	if( queue_empty( &usable ) == 1 )
	{
		err("http response header response queue empty\n");
		return ERROR;
	}
	q = queue_head( &usable );
	new = l_get_struct( q, http_response_head_t, queue );
	queue_remove( q );
	queue_insert_tail( &in_use, q );
	*response = new;
	return OK;
}

inline static void http_response_clear_string( string_t * str )
{
	str->len 	= 0;
	str->data	= NULL;
}

status http_response_head_free( http_response_head_t * response )
{
	response->state 			= 0;
	response->c 				= NULL;
	response->handler.process 	= NULL;
	response->handler.exit 		= NULL;

	http_response_clear_string( &response->header_key );
	http_response_clear_string( &response->header_value );
	http_response_clear_string( &response->http_code );
	http_response_clear_string( &response->http_version );
	http_response_clear_string( &response->http_string );

	response->http_status_code 	= 0;

	response->body_type 		= HTTP_BODY_TYPE_NULL;
	response->content_length 	= 0;
	response->keepalive 		= 0;
	response->head.pos 			= response->head.last = response->head.start;

	queue_remove( &response->queue );
	queue_insert_tail( &usable, &response->queue );
	return OK;
}

status http_response_head_create( connection_t * c, http_response_head_t ** response )
{
	http_response_head_t * new = NULL;

	if( OK != http_response_head_alloc( &new ) ) 
	{
		err( " response alloc");
		return ERROR;
	}
	new->c 					= c;
	new->state 				= 0;
	new->handler.process	= http_response_head_process_response_line;
	new->handler.exit		= http_response_head_free;

	*response = new;
	return OK;
}

status http_response_head_init_module ( void )
{
	uint32 i;

	queue_init( &in_use );
	queue_init( &usable );

	pool = (http_response_head_t*)l_safe_malloc( sizeof(http_response_head_t)* MAX_NET_CON );
	if( !pool ) 
	{
		err( " l_safe_malloc pool\n" );
		return ERROR;
	}
	memset( pool, 0, sizeof(http_response_head_t)*MAX_NET_CON );
	for( i = 0; i < MAX_NET_CON; i ++ )
	{
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}

status http_response_head_end_module( void )
{
	if( pool ) 
	{
		l_safe_free( pool );
	}
	return OK;
}

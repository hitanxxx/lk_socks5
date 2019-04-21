#include "lk.h"

static status json_parse_token( json_node_t * parent, json_ctx_t * ctx );
status json_free( json_node_t * json );

// json_node_alloc ---------
static status json_node_alloc( json_ctx_t * ctx, json_node_t ** json )
{
	json_node_t * new = NULL;

	new = l_mem_alloc( ctx->page, sizeof(json_node_t) );
	if( !new ) {
		err(" l_mem_alloc json node\n" );
		return ERROR;
	}
	*json = new;
	return OK;
}
// json_node_insert ---------
static status json_node_insert( json_ctx_t* ctx, json_node_t * parent, json_node_t ** node )
{
	json_node_t * new = NULL;

	if( !parent->child_flag ) {
		parent->child_flag = 1;
		queue_init( &parent->child );
	}
	if( OK != json_node_alloc( ctx, &new ) ) {
		return ERROR;
	}
	queue_insert_tail( &parent->child, &new->queue );
	new->parent = parent;
	*node = new;
	return OK;
}
// json_get_child -----
status json_get_child( json_node_t * parent, uint32 index, json_node_t ** child )
{
	queue_t * q;
	json_node_t * node;
	uint32 time = 1;

	if( !parent->child_flag ) {
		return ERROR;
	}
	for( q = queue_head( &parent->child ); q != queue_tail( &parent->child ); q = queue_next( q ), time ++ ) {
		if( index == time ) {
			node = l_get_struct( q, json_node_t, queue );
			*child = node;
			return OK;
		}
	}
	return ERROR;
}
// json_get_obj_child_by_name -----
status json_get_obj_child_by_name( json_node_t * parent, char * str, uint32 length, json_node_t ** child )
{
	json_node_t *key, *value;
	uint32 i;
	queue_t * q;

	if( parent->type != JSON_OBJ ) {
		return ERROR;
	}
	if( parent->child_flag ) {
		// traverse child
		for( q = queue_head( &parent->child ); q != queue_tail( &parent->child ); q = queue_next( q ) ) {
			key = l_get_struct( q, json_node_t, queue );
			if( OK == l_strncmp_cap( key->name.data, key->name.len, str, length ) ) {
				json_get_child( key, 1, &value );
				*child = value;
				return OK;
			}
		}
	}
	return ERROR;
}
// json_get_obj_str ---
status json_get_obj_str( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_STR ) {
		return ERROR;
	}
	return OK;
}
// json_get_obj_bool ---
status json_get_obj_bool( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_TRUE && (*child)->type != JSON_FALSE ) {
		return ERROR;
	}
	return OK;
}
// json_get_obj_num ---
status json_get_obj_num( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_NUM ) {
		return ERROR;
	}
	return OK;
}
// json_get_obj_null ----
status json_get_obj_null( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_NULL ) {
		return ERROR;
	}
	return OK;
}
// json_get_obj_arr ---
status json_get_obj_arr( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_ARR ) {
		return ERROR;
	}
	return OK;
}
// json_get_obj_obj ----
status json_get_obj_obj( json_node_t * obj, char * str, uint32 length, json_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->type != JSON_OBJ ) {
		return ERROR;
	}
	return OK;
}
// json_parse_true -------
static status json_parse_true( json_ctx_t * ctx )
{
	char * p;

	enum {
		t = 0,
		tr,
		tru,
		true
	} state;
	state = t;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == t ) {
			state = tr;
			continue;
		} else if ( state == tr ) {
			if( *p != 'r' ) {
				return ERROR;
			}
			state = tru;
			continue;
		} else if ( state == tru ) {
			if( *p != 'u' ) {
				return ERROR;
			}
			state = true;
			continue;
		} else if ( state == true ) {
			if( *p != 'e' ) {
				return ERROR;
			}
			return OK;
		}
	}
	return ERROR;
}
// json_parse_false ------
static status json_parse_false( json_ctx_t * ctx )
{
	char * p;

	enum {
		f = 0,
		fa,
		fal,
		fals,
		false
	} state;
	state = f;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == f ) {
			state = fa;
			continue;
		} else if ( state == fa ) {
			if( *p != 'a' ) {
				return ERROR;
			}
			state = fal;
			continue;
		} else if ( state == fal ) {
			if( *p != 'l' ) {
				return ERROR;
			}
			state = fals;
			continue;
		} else if ( state == fals ) {
			if( *p != 's' ) {
				return ERROR;
			}
			state = false;
			continue;
		} else if ( state == false ) {
			if( *p != 'e' ) {
				return ERROR;
			}
			return OK;
		}
	}
	return ERROR;
}
// json_parse_null -------
static status json_parse_null( json_ctx_t * ctx )
{
	char * p;

	enum {
		n = 0,
		nu,
		nul,
		null
	} state;
	state = n;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == n ) {
			state = nu;
			continue;
		} else if ( state == nu ) {
			if( *p != 'u' ) {
				return ERROR;
			}
			state = nul;
			continue;
		} else if ( state == nul ) {
			if( *p != 'l' ) {
				return ERROR;
			}
			state = null;
			continue;
		} else if ( state == null ) {
			if( *p != 'l' ) {
				return ERROR;
			}
			return OK;
		}
	}
	return ERROR;
}

// json_parse_string ----------
static status json_parse_string( json_node_t * json, json_ctx_t * ctx )
{
	char * p = NULL;

	enum {
		l_quotes = 0,
		string_start,
		string,
		transfer,
		transfer_ux,
		transfer_uxx,
		transfer_uxxx,
		transfer_uxxxx
	} state;
	state = l_quotes;
	for( ;ctx->p < ctx->end; ctx->p++ ) {
		p = ctx->p;

		if( state == l_quotes ) {
			state = string_start;
			continue;
		}
		if( state == string_start ) {
			json->name.data = p;
			if( *p == '\\' ) {
				state = transfer;
				continue;
			} else if( *p == '"' ) {
				json->name.len = meta_len( json->name.data, p );
				return OK;
			} else {
				state = string;
				continue;
			}
		}
		if( state == string ) {
			if( *p == '\\' ) {
				state = transfer;
				continue;
			} else if( *p == '"' ) {
				json->name.len = meta_len( json->name.data, p );
				return OK;
			}
		}
		if( state == transfer ) {
			if(
			*p == '\\'||
			*p == '/' ||
			*p == '"' ||
			*p == 'b' ||
			*p == 'f' ||
			*p == 'n' ||
			*p == 'r' ||
			*p == 't' ) {
				state = string;
				continue;
			} else if( *p == 'u' ) {
				state = transfer_ux;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == transfer_ux ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxx;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == transfer_uxx ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxxx;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == transfer_uxxx ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxxxx;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == transfer_uxxxx ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ) {
				state = string;
				continue;
			} else {
				return ERROR;
			}
		}
	}
	return ERROR;
}

// json_parse_obj_find_repeat ------
static status json_parse_obj_find_repeat( json_node_t * parent, json_node_t * child )
{
	queue_t * q;
	json_node_t * t;
	uint32 time = 0;

	if( parent->child_flag ) {
		for( q = queue_head( &parent->child ); q != queue_tail( &parent->child ); q = queue_next( q ) ) {
			t = l_get_struct( q, json_node_t, queue );
			if( OK == l_strncmp_cap( t->name.data, t->name.len, child->name.data, child->name.len ) ) {
				time ++;
			}
		}
	}
	if( time > 1 ) {
		return OK;
	}
	return ERROR;
}
// json_parse_obj ------------
static status json_parse_obj( json_node_t * json, json_ctx_t * ctx )
{
	json_node_t * child = NULL;
	char * p;

	enum {
		obj_start = 0,
		obj_name,
		obj_colon,
		obj_value,
		obj_part
	} state;
	state = obj_start;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == obj_start ) {
			state = obj_name;
			continue;
		}
		if( state == obj_name ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if( *p == '"' ) {
				if( OK != json_node_alloc( ctx, &child ) ) {
					return ERROR;
				}
				if( OK != json_parse_string( child, ctx ) ) {
					return ERROR;
				}
				// same level find repeat
				if( OK == json_parse_obj_find_repeat( json, child ) ) {
					err(" repeat\n" );
					return ERROR;
				}
				if( !json->child_flag ) {
					json->child_flag = 1;
					queue_init( &json->child );
				}
				queue_insert_tail( &json->child, &child->queue );
				state = obj_colon;
				continue;
			} else if( *p == '}' ) {
				return OK;
			} else {
				return ERROR;
			}
		}
		if( state == obj_colon ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if ( *p == ':' ) {
				state = obj_value;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == obj_value ) {
			if( OK != json_parse_token( child, ctx ) ) {
				return ERROR;
			}
			state = obj_part;
			continue;
		}
		if( state == obj_part ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if ( *p == ',' ) {
				state = obj_name;
				continue;
			} else if ( *p == '}' ) {
				return OK;
			} else {
				return ERROR;
			}
		}
	}
	return ERROR;
}

// json_parse_array -----------
static status json_parse_array ( json_node_t * json, json_ctx_t * ctx )
{
	char * p;

	enum {
		arr_start = 0,
		arr_value_start,
		arr_value,
		arr_part,
		arr_end
	} state;
	state = arr_start;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == arr_start ) {
			state = arr_value_start;
			continue;
		}
		if( state == arr_value_start ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if ( *p == ']' ) {
				return OK;
			} else {
				if( OK != json_parse_token( json, ctx ) ) {
					return ERROR;
				}
				state = arr_part;
				continue;
			}
		}
		if( state == arr_part ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if( *p == ',' ) {
				state = arr_value;
				continue;
			} else if( *p == ']' ) {
				return OK;
			} else {
				return ERROR;
			}
		}
		if( state == arr_value ) {
			if(
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else {
				if( OK != json_parse_token( json, ctx ) ) {
					return ERROR;
				}
				state = arr_part;
				continue;
			}
		}
	}
	return ERROR;
}
/// json_parse_num ------------
static status json_parse_num ( json_node_t * json, json_ctx_t * ctx )
{
	char * p = NULL;
	string_t num_string;
	char * end;

	num_string.data = NULL;
	num_string.len = 0;

	enum {
		num_start = 0,
		inte_string_start,
		inte_string,
		inte_before_end,
		dec_string_start,
		dec_string,
		dec_before_end,
		over
	} state;
	state = num_start;
	for( ; ctx->p < ctx->end; ctx->p++ ) {
		p = ctx->p;

		if( state == num_start ) {
			if( *p == ' ' ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if (
			*p == '+' ||
			*p == '-' ) {
				num_string.data = p;
				state = inte_string_start;
				continue;
			} else if ( ( *p >= '0' && *p <= '9' ) ) {
				num_string.data = p;
				state = inte_string_start;
			} else {
				return ERROR;
			}
		}
		if( state == inte_string_start ) {
			if( *p >= '0' && *p <= '9' ) {
				state = inte_string;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == inte_string ) {
			if( *p >= '0' && *p <= '9' ) {
				continue;
			} else if ( *p == '.' ) {
				state = dec_string_start;
				continue;
			} else if ( *p == ',' || *p == '}' || *p == ']' ) {
				state = over;
			} else {
				state = inte_before_end;
				continue;
			}
		}
		if( state == inte_before_end ) {
			if( *p == ' ' ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if( *p == ',' || *p == '}' || *p == ']' ) {
				state = over;
			} else {
				return ERROR;
			}
		}
		if( state == dec_string_start ) {
			if( *p >= '0' && *p <= '9' ) {
				state = dec_string;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == dec_string ) {
			if( *p >= '0' && *p <= '9' ) {
				continue;
			} else if ( *p == ',' || *p == '}' || *p == ']' ) {
				state = over;
			} else {
				state = dec_before_end;
				continue;
			}
		}
		if( state == dec_before_end ) {
			if( *p == ' ' ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
				continue;
			} else if ( *p == ',' || *p == '}' || *p == ']' ) {
				state = over;
			} else {
				return ERROR;
			}
		}
		if( state == over ) {
			num_string.len = meta_len( num_string.data, p );

			end = num_string.data + num_string.len;
			json->num = strtod( num_string.data, &end );
			if( errno == ERANGE && ( json->num == HUGE_VAL || json->num == -HUGE_VAL ) ) {
				err ( " number is too big or to less\n" );
				return ERROR;
			}
			if( num_string.data == end ) {
				err ( " number str empty\n" );
				return ERROR;
			}

			ctx->p-=1;
			return OK;
		}
	}
	if( json->parent->type == JSON_ROOT ) {
		num_string.len = meta_len( num_string.data, p );

		end = num_string.data + num_string.len;
		json->num = strtod( num_string.data, &end );
		if( errno == ERANGE && ( json->num == HUGE_VAL || json->num == -HUGE_VAL ) ) {
			err ( " number is too big or to less\n" );
			return ERROR;
		}
		if( num_string.data == end ) {
			err ( " number str empty\n" );
			return ERROR;
		}
		return OK;
	}
	return ERROR;
}

// json_parse_token -----------
static status json_parse_token( json_node_t * parent, json_ctx_t * ctx )
{
	json_node_t * json = NULL;
	char * p;

	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;
		if( *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ) {
			continue;
		} else if( *p == 't' ) {
			if( OK != json_parse_true( ctx ) ) {
				err("parse 'true' failed\n");
				goto failed;
			}
			json_node_insert( ctx, parent, &json );
			json->type = JSON_TRUE;
			return OK;

		} else if( *p == 'f' ) {
			if( OK != json_parse_false( ctx ) ) {
				err("parse 'false' failed\n");
				goto failed;
			}
			json_node_insert( ctx, parent, &json );
			json->type = JSON_FALSE;
			return OK;

		} else if( *p == 'n' ) {
			if( OK != json_parse_null( ctx ) ) {
				err("parse 'null' failed\n");
				goto failed;
			}
			json_node_insert( ctx, parent, &json );
			json->type = JSON_NULL;
			return OK;

		} else if( *p == '"' ) {
			json_node_insert( ctx, parent, &json );
			if( OK != json_parse_string( json, ctx ) ) {
				err("parse string failed\n");
				goto failed;
			}
			json->type = JSON_STR;
			return OK;

		} else if( *p == '{' ) {
			json_node_insert( ctx, parent, &json );
			if( OK != json_parse_obj( json, ctx ) ) {
				err("parse obj failed\n");
				goto failed;
			}
			json->type = JSON_OBJ;
			return OK;

		} else if( *p == '[' ) {
			json_node_insert( ctx, parent, &json );
			if( OK != json_parse_array( json, ctx ) ) {
				err("parse array failed\n");
				goto failed;
			}
			json->type = JSON_ARR;
			return OK;

		} else {
			json_node_insert( ctx, parent, &json );
			if( OK != json_parse_num( json, ctx ) ) {
				err("parse number failed\n");
				goto failed;
			}
			json->type = JSON_NUM;
			return OK;
		}
	}
failed:
	return ERROR;
}

// json_decode_check ---------
static status json_decode_check( json_ctx_t * ctx )
{
	char * p;

	ctx->p++;
	for( ; ctx->p < ctx->end; ctx->p++ ) {
		p = ctx->p;
		if( *p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
			continue;
		} else {
			err(" after parse token, illegal [%d]\n", *p );
			return ERROR;
		}
	}
	return OK;
}
// josn_ctx_create -----------
status json_ctx_create( json_ctx_t ** json_ctx )
{
	json_ctx_t * new = NULL;

	new = l_safe_malloc( sizeof(json_ctx_t) );
	if( !new ) {
		err(" l_safe_malloc json ctx\n" );
		return ERROR;
	}
	memset( new, 0, sizeof(json_ctx_t) );
	if( OK != l_mem_page_create( &new->page, 4096 ) ) {
		err(" l_mem_page_alloc json ctx page\n" );
		return ERROR;
	}
	new->root.type = JSON_ROOT;
	*json_ctx = new;
	return OK;
}
// json_ctx_free -----------
status json_ctx_free( json_ctx_t * ctx )
{
	if( ctx ) {
		if( ctx->page ) {
			if( OK != l_mem_page_free( ctx->page ) ) {
				err(" l_mem_page_free ctx page failed\n" );
				return ERROR;
			}
		}
		ctx->page = NULL;
		l_safe_free( ctx );
	}
	return OK;
}
// json_decode ------------
status json_decode( json_ctx_t * ctx, char * p, char * end )
{
	ctx->p = p;
	ctx->end = end;

	if( OK != json_parse_token( &ctx->root, ctx ) ) {
		return ERROR;
	}
	if( OK != json_decode_check( ctx ) ) {
		return ERROR;
	}
	return OK;
}
// json_add_obj -----
json_node_t * json_add_obj( json_ctx_t * ctx, json_node_t * parent )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_OBJ;
	return child;
}
// json_add_arr ------
json_node_t * json_add_arr( json_ctx_t * ctx, json_node_t * parent )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_ARR;
	return child;
}
// json_add_true -----
json_node_t * json_add_true( json_ctx_t * ctx, json_node_t * parent )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_TRUE;
	return child;
}
// json_add_false ------
json_node_t * json_add_false ( json_ctx_t * ctx, json_node_t * parent )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_FALSE;
	return child;
}
// json_add_null ------
json_node_t * json_add_null( json_ctx_t * ctx, json_node_t * parent )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_NULL;
	return child;
}
// json_add_str ------
json_node_t * json_add_str( json_ctx_t * ctx, json_node_t * parent, char * str, uint32 length )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_STR;
	child->name.data = str;
	child->name.len = length;
	return child;
}
// json_add_num -------
json_node_t * json_add_num ( json_ctx_t * ctx, json_node_t * parent, uint32 num )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_NUM;
	child->num = (double)num;
	return child;
}
// json_obj_add_child -------
json_node_t * json_obj_add_child( json_ctx_t * ctx, json_node_t * parent, char * str, uint32 length )
{
	json_node_t * child;
	if( OK != json_node_insert( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->type = JSON_STR;
	child->type = 0;
	child->name.data = str;
	child->name.len = length;
	return child;
}
// json_stringify_token_len -------
static uint32 json_stringify_token_len( json_node_t * json )
{
	uint32 len = 0;
	uint32 i;
	json_node_t * temp;
	char str[100];
	queue_t * q;

	if( json->type == JSON_TRUE ) {
		return 4;
	} else if( json->type == JSON_FALSE ) {
		return 5;
	} else if( json->type == JSON_NULL ) {
		return 4;
	} else if( json->type == JSON_STR ) {
		len += json->name.len;
		len += 2;
		return len;
	} else if( json->type == JSON_OBJ ) {
		len += 1; // {

		if( json->child_flag ) {
			for( q = queue_head( &json->child ); q != queue_tail( &json->child ); q = queue_next(q) ) {
				len += 1; // "
				temp = l_get_struct( q, json_node_t, queue );
				len += temp->name.len;
				len += 1; // "
				len += 1; // :
				if( OK != json_get_child( temp, 1, &temp ) ) {
					return OK;
				}
				len += json_stringify_token_len( temp );
				len += 1; // ,
			}
			len --;
		}
		len += 1; // }
		return len;
	} else if( json->type == JSON_ARR ) {
		len += 1; // [
		if( json->child_flag ) {
			for( q = queue_head( &json->child ); q != queue_tail( &json->child ); q = queue_next(q) ) {
				temp = l_get_struct( q, json_node_t, queue );
				len += json_stringify_token_len( temp );
				len += 1; // ,
			}
			len--;
		}
		len += 1; // ]
		return len;
	} else if ( json->type == JSON_NUM ) {
		memset( str, 0, sizeof(str) );
		sprintf( str, "%.2f\n", json->num );
		len += l_strlen( str );
		return len;
	}
	return 0;
}
// json_stringify_token -------
static char* json_stringify_token( json_node_t * json, char * p )
{
	uint32 i;
	json_node_t * temp;
	char str[100];
	queue_t * q;

	if( json->type == JSON_TRUE ) {
		*p = 't';
		*(p+1) = 'r';
		*(p+2) = 'u';
		*(p+3) = 'e';
		p+=4;
		return p;
	} else if( json->type == JSON_FALSE ) {
		*p = 'f';
		*(p+1) = 'a';
		*(p+2) = 'l';
		*(p+3) = 's';
		*(p+4) = 'e';
		p+=5;
		return p;
	} else if( json->type == JSON_NULL ) {
		*p = 'n';
		*(p+1) = 'u';
		*(p+2) = 'l';
		*(p+3) = 'l';
		p+=4;
		return p;
	} else if( json->type == JSON_STR ) {
		*p = '"';
		p++;
		memcpy( p, json->name.data, json->name.len );
		p+= json->name.len;
		*p = '"';
		p++;
		return p;
	} else if( json->type == JSON_OBJ ) {
		*p = '{';
		p++;

		if( json->child_flag ) {
			for( q = queue_head( &json->child ); q != queue_tail( &json->child ); q = queue_next(q) ) {
				*p = '"';
				p++;
				temp = l_get_struct( q, json_node_t, queue );
				memcpy( p, temp->name.data, temp->name.len );
				p +=  temp->name.len;
				*p = '"';
				p++;
				*p = ':';
				p++;
				json_get_child( temp, 1, &temp );
				p = json_stringify_token( temp ,p );
				*p = ',';
				p++;
			}
			p --;
		}
		*p = '}';
		p++;
		return p;
	} else if( json->type == JSON_ARR ) {
		*p = '[';
		p++;
		if( json->child_flag ) {
			for( q = queue_head( &json->child ); q != queue_tail( &json->child ); q = queue_next(q) ) {
				temp = l_get_struct( q, json_node_t, queue );
				p = json_stringify_token( temp, p );
				if( NULL == p ) return NULL;
				*p = ',';
				p++;
			}
			p--;
		}
		*p = ']';
		p++;
		return p;
	} else if ( json->type == JSON_NUM ) {
		memset( str, 0, sizeof(str) );
		sprintf( str, "%.2f\n", json->num );
		memcpy( p, str, strlen(str) );
		p += strlen(str);
		return p;
	}
	return NULL;
}
// json_encode ----------
status json_encode( json_ctx_t * ctx, meta_t ** string )
{
	uint32 len;
	json_node_t * root;
	meta_t * meta;

	root = &ctx->root;
	json_get_child( root, 1, &root );
	len = json_stringify_token_len( root );
	if( len == 0 ) {
		return ERROR;
	}
	if( OK != meta_alloc( &meta, len ) ) {
		return ERROR;
	}
	meta->last = json_stringify_token( root, meta->pos );
	if( !meta->last ) {
		meta_free( meta );
		return ERROR;
	} else if( meta->last != meta->end ) {
		meta_free( meta );
		return ERROR;
	}
	*string = meta;
	return OK;
}

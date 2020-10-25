#include "l_base.h"

static status json_parse_token( ljson_node_t * parent, ljson_ctx_t * ctx );

#if(1)
static status json_node_alloc( ljson_ctx_t * ctx, ljson_node_t ** json )
{
	ljson_node_t * new = NULL;

	new = l_mem_alloc( ctx->page, sizeof(ljson_node_t) );
	if( !new ) {
		err(" l_mem_alloc ljson node\n" );
		return ERROR;
	}
	memset( new, 0, sizeof(ljson_node_t) );
	*json = new;
	return OK;
}

static status json_node_add( ljson_ctx_t* ctx, ljson_node_t * parent, ljson_node_t ** node )
{
	ljson_node_t * new = NULL;

	if( OK != json_node_alloc( ctx, &new ) ) {
		return ERROR;
	}

	if( parent->child == NULL ) {
		parent->child = new;
		new->prev = NULL;
	} else {
		parent->child_last->next = new;
		new->prev = parent->child_last;
		
	}
	parent->child_last = new;
	new->parent = parent;

	*node = new;
	return OK;
}
#endif

status json_get_child( ljson_node_t * parent, uint32 index, ljson_node_t ** child )
{
	ljson_node_t * node;
	uint32 time = 1;

	if( parent == NULL )
	{
		return ERROR;
	}

	node = parent->child;
	while( node )
	{
		if( index == time )
		{
			*child = node;
			return OK;
		}
		node = node->next;
		time ++;
	}
	return ERROR;
}

status json_get_obj_child_by_name( ljson_node_t * parent, char * str, uint32 length, ljson_node_t ** child )
{
	ljson_node_t *obj, *obj_frist_child;

	if( parent == NULL )
	{	
		err("parent point NULL\n");
		return ERROR;
	}

	obj = parent->child;
	while( obj )
	{
		if( OK == l_strncmp_cap(obj->name.data, obj->name.len, (unsigned char*)str, length ) )
		{
			if( OK != json_get_child( obj, 1, &obj_frist_child ) )
			{
				err("json obj node have't frist child\n");
				return ERROR;
			}
			if( child )
			{
				*child = obj_frist_child;
			}
			return OK;
		}
		obj = obj->next;
	}
	return ERROR;
}

status json_get_obj_str( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_STR ) {
		return ERROR;
	}
	return OK;
}

status json_get_obj_bool( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_TRUE && (*child)->node_type != JSON_FALSE ) {
		return ERROR;
	}
	return OK;
}

status json_get_obj_num( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_NUM ) {
		return ERROR;
	}
	return OK;
}

status json_get_obj_null( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_NULL ) {
		return ERROR;
	}
	return OK;
}

status json_get_obj_arr( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_ARR ) {
		return ERROR;
	}
	return OK;
}

status json_get_obj_obj( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child )
{
	if( OK != json_get_obj_child_by_name( obj, str, length, child ) ) {
		return ERROR;
	}
	if( (*child)->node_type != JSON_OBJ ) {
		return ERROR;
	}
	return OK;
}

#if(1)
static status json_parse_true( ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

	enum {
		t = 0,
		tr,
		tru,
		true
	} state;
	state = t;
	for( ; ctx->p < ctx->end; ctx->p ++ )
    {
		p = ctx->p;

		if( state == t )
        {
			state = tr;
			continue;
		}
        else if ( state == tr )
        {
			if( *p != 'r' )
            {
                break;
			}
			state = tru;
			continue;
		}
        else if ( state == tru )
        {
			if( *p != 'u' )
            {
                break;
			}
			state = true;
			continue;
		}
        else if ( state == true )
        {
			if( *p != 'e' )
            {
                break;
			}
			return OK;
		}
	}
    err("lsjon true unexpect [%c]\n", *p );
	return ERROR;
}

static status json_parse_false( ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

	enum {
		f = 0,
		fa,
		fal,
		fals,
		false
	} state;
	state = f;
	for( ; ctx->p < ctx->end; ctx->p ++ )
    {
		p = ctx->p;

		if( state == f )
        {
			state = fa;
			continue;
		}
        else if ( state == fa )
        {
			if( *p != 'a' )
            {
                break;
			}
			state = fal;
			continue;
		}
        else if ( state == fal )
        {
			if( *p != 'l' )
            {
                break;
			}
			state = fals;
			continue;
		}
        else if ( state == fals )
        {
			if( *p != 's' )
            {
                break;
			}
			state = false;
			continue;
		}
        else if ( state == false )
        {
			if( *p != 'e' )
            {
                break;
			}
			return OK;
		}
	}
    err("ljson false unexpect [%c]\n", *p );
	return ERROR;
}

static status json_parse_null( ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

	enum {
		n = 0,
		nu,
		nul,
		null
	} state;
	state = n;
	for( ; ctx->p < ctx->end; ctx->p ++ )
    {
		p = ctx->p;

		if( state == n )
        {
			state = nu;
			continue;
		}
        else if ( state == nu )
        {
			if( *p != 'u' )
            {
                break;
			}
			state = nul;
			continue;
		}
        else if ( state == nul )
        {
			if( *p != 'l' )
            {
				break;
			}
			state = null;
			continue;
		}
        else if ( state == null )
        {
			if( *p != 'l' )
            {
				break;
			}
			return OK;
		}
	}
    err("ljson null unexpect [%c]\n", *p );
	return ERROR;
}

static status json_parse_string( ljson_node_t * json, ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

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
	for( ;ctx->p < ctx->end; ctx->p++ )
    {
		p = ctx->p;

		if( state == l_quotes ) 
		{
			state = string_start;
			continue;
		}
		if( state == string_start )
		{
			json->name.data = p;
			if( *p == '\\' ) {
				state = transfer;
				continue;
			} else if( *p == '"' ) {
				json->name.len = meta_len( json->name.data, p );
				return OK;
			}
			state = string;
			continue;
		}
		if( state == string ) 
		{
			if( *p == '\\' ) {
				state = transfer;
				continue;
			} else if( *p == '"' ) {
				json->name.len = meta_len( json->name.data, p );
				return OK;
			}
		}
		if( state == transfer ) 
		{
			if(
				*p == '"' ||
				*p == '\\'||
				*p == '/' ||
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
			}
            err("ljson string state transfer unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == transfer_ux ) 
		{
			if(
				( *p >= '0' && *p <= '9' ) ||
				( *p >= 'a' && *p <= 'f' ) ||
				( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxx;
				continue;
			}
            err("ljson string state transfer ux unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == transfer_uxx ) 
		{
			if(
				( *p >= '0' && *p <= '9' ) ||
				( *p >= 'a' && *p <= 'f' ) ||
				( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxxx;
				continue;
			}
            err("ljson string state transfer uxx unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == transfer_uxxx ) 
		{
			if(
				( *p >= '0' && *p <= '9' ) ||
				( *p >= 'a' && *p <= 'f' ) ||
				( *p >= 'A' && *p <= 'F' ) ) {
				state = transfer_uxxxx;
				continue;
			}
            err("ljson string state transfer uxxx unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == transfer_uxxxx ) 
		{
			if(
				( *p >= '0' && *p <= '9' ) ||
				( *p >= 'a' && *p <= 'f' ) ||
				( *p >= 'A' && *p <= 'F' ) ) {
				state = string;
				continue;
			}
            err("ljson string state transfer uxxxx unexpect [%c]\n", *p );
			return ERROR;
		}
	}
    err("ljson string unexpect [%c]\n", *p );
	return ERROR;
}


static status json_parse_obj_find_repeat( ljson_node_t * parent, ljson_node_t * child )
{
	ljson_node_t * t = parent->child;
	uint32 time = 0;

	while( t )
	{
		if( OK == l_strncmp_cap( t->name.data, t->name.len, child->name.data, child->name.len ) ) {
			time ++;
		}
		if( time > 1 ) {
			return OK;
		}
		t = t->next;
	}
	return ERROR;
}

static status json_parse_obj( ljson_node_t * parent, ljson_ctx_t * ctx )
{
	ljson_node_t * child = NULL;
	unsigned char * p = NULL;

	enum {
		obj_start = 0,
		obj_name,
        obj_comma,
		obj_colon,
		obj_value,
		obj_part
	} state;
	state = obj_start;
	for( ; ctx->p < ctx->end; ctx->p ++ )
    {
		p = ctx->p;

		if( state == obj_start ) {
			state = obj_name;
			continue;
		}
        if( state == obj_comma )
        {
            if(
               *p == ' '  ||
               *p == '\r' ||
               *p == '\n' ||
               *p == '\t' ) {
                continue;
            }
            else if( *p == '"' )
            {
                state = obj_name;
            }
            else
            {
                err("ljson obj comma unexpect [%c]\n", *p );
                return ERROR;
            }
        }
		if( state == obj_name ) {
			if(
				*p == ' '  ||
				*p == '\r' ||
				*p == '\n' ||
				*p == '\t' ) {
				continue;
			} 
			else if( *p == '"' ) 
			{
				if( OK != json_node_add( ctx, parent, &child ) ) {
                    err("ljson obj create new node failed\n");
					return ERROR;
				}
				if( OK != json_parse_string( child, ctx ) ) {
					return ERROR;
				}
				// after parse string, p is '"'
				
				// same level find repeat
				if( OK == json_parse_obj_find_repeat( parent, child ) ) {
					err("ljson obj child repeat\n" );
					return ERROR;
				}
				state = obj_colon;
				continue;
			} 
			else if( *p == '}' ) 
			{
				return OK;
			}
            err("ljson obj state name unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == obj_colon ) 
		{
			if(
				*p == ' '  ||
				*p == '\r' ||
				*p == '\n' ||
				*p == '\t' ) {
				continue;
			} else if ( *p == ':' ) {
				state = obj_value;
				continue;
			}
            err("ljson obj colon unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == obj_value ) 
		{
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
				state = obj_comma;
				continue;
			} else if ( *p == '}' ) {
				return OK;
			} else {
                err("ljson obj part unexpect [%c]\n", *p );
				return ERROR;
			}
		}
	}
    err("ljson obj unexpect [%c]\n", *p );
	return ERROR;
}


static status json_parse_array ( ljson_node_t * arr, ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

	enum {
		arr_start = 0,
        arr_comma,
		arr_value,
		arr_part	
	} state;
	state = arr_start;
	for( ; ctx->p < ctx->end; ctx->p ++ ) {
		p = ctx->p;

		if( state == arr_start ) 
		{
			state = arr_value;
			continue;
		}
		if( state == arr_part ) 
		{
			if(
				*p == ' '  ||
				*p == '\r' ||
				*p == '\n' ||
				*p == '\t' ) 
			{
				continue;
			} 
			else if( *p == ',' ) 
			{
				state = arr_comma;
				continue;
			} 
			else if( *p == ']' ) 
			{
				arr->node_type	= JSON_ARR;
				return OK;
			}
            err("ljson arr part unexpect [%c]\n", *p );
			return ERROR;
		}
        if( state == arr_comma )
        {
            if(
               *p == ' '  ||
               *p == '\r' ||
               *p == '\n' ||
               *p == '\t' )
            {
                continue;
            }
            else if ( *p == ']' )
            {
                err("ljson arr comma unexpect [%c]\n", *p );
                return ERROR;
            }
            if( OK != json_parse_token( arr, ctx ) ) {
                return ERROR;
            }
            state = arr_part;
            continue;
        }
		if( state == arr_value ) 
		{
			if(
				*p == ' '  ||
				*p == '\r' ||
				*p == '\n' ||
				*p == '\t' ) 
			{
				continue;
			} 
			else if ( *p == ']' )
			{
				arr->node_type	= JSON_ARR;
				return OK;
			} 
			if( OK != json_parse_token( arr, ctx ) ) {
				return ERROR;
			}
			state = arr_part;
			continue;
		}
	}
    err("ljson arr unexpect [%c]\n", *p );
	return ERROR;
}

static status json_parse_num ( ljson_node_t * json, ljson_ctx_t * ctx )
{
	unsigned char * p = NULL, * end = NULL;
	string_t num_string = string_null;

	enum {
		num_start = 0,
		inte_string_start,
		inte_string,
		inte_dec_before_end,
		dec_string_start,
		dec_string,
		over
	} state;
	state = num_start;
	for( ; ctx->p < ctx->end; ctx->p++ ) {
		p = ctx->p;

		if( state == num_start ) 
		{
			if(  *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ) 
			{
				continue;
			} 
			else if ( *p == '+' || *p == '-' ) 
			{
				num_string.data = p;
				state = inte_string_start;
				continue;
			} 
			else if ( ( *p >= '0' && *p <= '9' ) )
			{
				num_string.data = p;
				state = inte_string;
				continue;
			}
            err("ljson num start unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == inte_string_start ) 
		{
			if( *p >= '0' && *p <= '9' ) 
			{
				state = inte_string;
				continue;
			}
            err("ljson inte_string_start unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == inte_string ) 
		{
			if( *p >= '0' && *p <= '9' ) 
			{
				continue;
			} 
			else if ( *p == '.' ) 
			{
				state = dec_string_start;
				continue;
			}
			state = inte_dec_before_end;
		}
		if( state == inte_dec_before_end ) 
		{
			if( *p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ) 
			{
				continue;
			} 
			else if( *p == ',' || *p == '}' || *p == ']' )
			{
				state = over;
			}
			else
			{
                err("ljson num inte_dec_before_end unexpect [%c]\n", *p );
				return ERROR;
			}
		}
		if( state == dec_string_start ) 
		{
			if( *p >= '0' && *p <= '9' ) 
			{
				state = dec_string;
				continue;
			}
            err("ljson dec_string_start unexpect [%c]\n", *p );
			return ERROR;
		}
		if( state == dec_string ) 
		{
			if( *p >= '0' && *p <= '9' ) 
			{
				continue;
			} 
			state = inte_dec_before_end;
		}
		if( state == over ) 
		{
			num_string.len = meta_len( num_string.data, p );
			end = num_string.data + num_string.len;
			
			json->num_d = strtod( (char*)num_string.data, (char**)&end );
			json->num_f = (float)json->num_d;
			json->num_i = (int)json->num_f;
			
			if( errno == ERANGE && ( json->num_d == HUGE_VAL || json->num_d == -HUGE_VAL ) ) {
				err("ljson num is too big or to less\n");
				return ERROR;
			}
			if( num_string.data == end ) {
				err("ljson num str empty\n" );
				return ERROR;
			}
			ctx->p-=1;
			return OK;
		}
	}

	// if just have num string
	if( json->parent->node_type == JSON_ROOT ) {
		num_string.len = meta_len( num_string.data, p );
		end = num_string.data + num_string.len;
	
		json->num_d = strtod( (char*)num_string.data, (char**)&end );
		json->num_f = (float)json->num_d;
		json->num_i = (int)json->num_f;
		
		if( errno == ERANGE && ( json->num_d == HUGE_VAL || json->num_d == -HUGE_VAL ) ) {
			err("ljson num is too big or to less\n");
			return ERROR;
		}
		if( num_string.data == end ) {
			err("ljson num str empty\n" );
			return ERROR;
		}
		return OK;
	}
	return ERROR;
}

static status json_parse_token( ljson_node_t * parent, ljson_ctx_t * ctx )
{
	ljson_node_t * cur_node = NULL;
	unsigned char * p = NULL;

	for( ; ctx->p < ctx->end; ctx->p ++ )
    {
		p = ctx->p;
		if( 
			*p == ' '  ||
			*p == '\r' ||
			*p == '\n' ||
			*p == '\t' ) {
			continue;
		} 
		else if( *p == 't' ) 
		{
			if( OK != json_parse_true( ctx ) ) 
			{
                break;
			}
			// json_node_add will alloc a new structrue
			json_node_add( ctx, parent, &cur_node );
			cur_node->node_type = JSON_TRUE;
			return OK;
		} 
		else if( *p == 'f' ) 
		{
			if( OK != json_parse_false( ctx ) ) {
                break;
			}
			json_node_add( ctx, parent, &cur_node );
			cur_node->node_type = JSON_FALSE;
			return OK;

		} 
		else if( *p == 'n' ) 
		{
			if( OK != json_parse_null( ctx ) ) {
                break;
			}
			json_node_add( ctx, parent, &cur_node );
			cur_node->node_type = JSON_NULL;
			return OK;

		} 
		else if( *p == '"' ) 
		{
			json_node_add( ctx, parent, &cur_node );
			if( OK != json_parse_string( cur_node, ctx ) ) {
                break;
			}
			cur_node->node_type = JSON_STR;
			return OK;

		} 
		else if( *p == '{' ) 
		{
			json_node_add( ctx, parent, &cur_node );
			if( OK != json_parse_obj( cur_node, ctx ) ) {
                break;
			}
			cur_node->node_type = JSON_OBJ;
			return OK;
		} 
		else if( *p == '[' ) 
		{	
			json_node_add( ctx, parent, &cur_node );
			if( OK != json_parse_array( cur_node, ctx ) ) {
                break;
			}
			return OK;

		} else {
			json_node_add( ctx, parent, &cur_node );
			if( OK != json_parse_num( cur_node, ctx ) ) {
                break;
			}
			cur_node->node_type = JSON_NUM;
			return OK;
		}
	}
	return ERROR;
}
#endif
static status json_decode_check( ljson_ctx_t * ctx )
{
	unsigned char * p = NULL;

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

status json_decode( ljson_ctx_t * ctx, unsigned char * p, unsigned char * end )
{
	ctx->p      = p;
	ctx->end    = end;

	if( OK != json_parse_token( &ctx->root, ctx ) ) {
		return ERROR;
	}
	if( OK != json_decode_check( ctx ) ) {
		return ERROR;
	}
	return OK;
}


status json_ctx_create( ljson_ctx_t ** json_ctx )
{
	ljson_ctx_t * new = NULL;

	new = l_safe_malloc( sizeof(ljson_ctx_t) );
	if( !new ) {
		err(" l_safe_malloc ljson ctx\n" );
		return ERROR;
	}
	memset( new, 0, sizeof(ljson_ctx_t) );
	
	if( OK != l_mem_page_create( &new->page, L_PAGE_DEFAULT_SIZE ) ) {
		err(" l_mem_page_alloc json ctx page\n" );
		return ERROR;
	}
	new->root.node_type = JSON_ROOT;
	new->root.child 	= NULL;
	new->root.child_last = NULL;
	new->root.parent	= NULL;
	new->root.next		= NULL;
	new->root.prev 		= NULL;
	
	*json_ctx = new;
	return OK;
}

status json_ctx_free( ljson_ctx_t * ctx )
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

#if(1)

ljson_node_t * json_add_obj( ljson_ctx_t * ctx, ljson_node_t * parent )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_OBJ;
	return child;
}

ljson_node_t * json_add_arr( ljson_ctx_t * ctx, ljson_node_t * parent )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_ARR;
	return child;
}

ljson_node_t * json_add_true( ljson_ctx_t * ctx, ljson_node_t * parent )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_TRUE;
	return child;
}

ljson_node_t * json_add_false ( ljson_ctx_t * ctx, ljson_node_t * parent )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_FALSE;
	return child;
}

ljson_node_t * json_add_null( ljson_ctx_t * ctx, ljson_node_t * parent )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_NULL;
	return child;
}

ljson_node_t * json_add_str( ljson_ctx_t * ctx, ljson_node_t * parent, unsigned char * str, uint32 length )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type    = JSON_STR;
	child->name.data    = str;
	child->name.len     = length;
	return child;
}

ljson_node_t * json_add_int ( ljson_ctx_t * ctx, ljson_node_t * parent, int32 num )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	child->node_type = JSON_NUM;
	child->num_i = num;
	child->num_f = (float)child->num_i;
	child->num_d = (double)child->num_i;
	return child;
}

ljson_node_t * json_obj_add_child( ljson_ctx_t * ctx, ljson_node_t * parent, unsigned char * str, uint32 length )
{
	ljson_node_t * child;
	if( OK != json_node_add( ctx, parent, &child ) ) {
		err(" json node insert\n" );
		return NULL;
	}
	//child->node_type = JSON_STR;
	child->node_type    = 0;
	child->name.data    = str;
	child->name.len     = length;
	return child;
}
#endif


static uint32 json_stringify_token_len( ljson_node_t * json )
{
	uint32 len = 0;
	ljson_node_t * temp, * temp1;
	char str[100];

	if( json == NULL 
){
		return 0;
	}

	if( json->node_type == JSON_TRUE ) 
	{
		return 4;
	} 
	else if( json->node_type == JSON_FALSE )
	{
		return 5;
	} 
	else if( json->node_type == JSON_NULL )
	{
		return 4;
	} 
	else if( json->node_type == JSON_STR ) 
	{
		len += json->name.len;
		len += 2;
		return len;
	} 
	else if( json->node_type == JSON_OBJ ) 
	{
		len += 1; // {

		temp = json->child;
		while( temp )
		{
			len += 1; // "
			len += temp->name.len;
			len += 1; // "
			len += 1; // :

			temp1 = temp->child;
			len += json_stringify_token_len( temp1 );
			
			len += 1; // ,
			temp = temp->next;
		}
		len --;
	
		len += 1; // }
		return len;
	} 
	else if( json->node_type == JSON_ARR ) 
	{
		len += 1; // [

		temp = json->child;
		while( temp )
		{
			len += json_stringify_token_len( temp );
			len += 1; // ,
			temp = temp->next;
		}
		len--;

		len += 1; // ]
		return len;
	} 
	else if ( json->node_type == JSON_NUM ) 
	{
		memset( str, 0, sizeof(str) );
		sprintf( str, "%.2f\n", json->num_f );
		len += l_strlen( str );
		return len;
	}
	return 0;
}

static char* json_stringify_token( ljson_node_t * json, char * p )
{
	ljson_node_t * temp, *temp1;
	char str[100];

	if( json == NULL ){
		return p;
	}

	if( json->node_type == JSON_TRUE ) 
	{
		*p = 't';
		*(p+1) = 'r';
		*(p+2) = 'u';
		*(p+3) = 'e';
		p+=4;
		return p;
	} 
	else if( json->node_type == JSON_FALSE ) 
	{
		*p = 'f';
		*(p+1) = 'a';
		*(p+2) = 'l';
		*(p+3) = 's';
		*(p+4) = 'e';
		p+=5;
		return p;
	} 
	else if( json->node_type == JSON_NULL )
	{
		*p = 'n';
		*(p+1) = 'u';
		*(p+2) = 'l';
		*(p+3) = 'l';
		p+=4;
		return p;
	} 
	else if( json->node_type == JSON_STR )
	{
		*p = '"';
		p++;
		memcpy( p, json->name.data, json->name.len );
		p+= json->name.len;
		*p = '"';
		p++;
		return p;
	} 
	else if( json->node_type == JSON_OBJ ) 
	{
		*p = '{';
		p++;

		temp = json->child;
		while( temp )
		{
			*p = '"';
			p++;

			memcpy( p, temp->name.data, temp->name.len );
			p +=  temp->name.len;

			*p = '"';
			p++;

			*p = ':';
			p++;

			temp1 = temp->child;
			p = json_stringify_token( temp1, p );
			
			*p = ',';
			p++;
			
			temp = temp->next;
		}
		p --;
		
		*p = '}';
		p++;
		return p;
	} 
	else if( json->node_type == JSON_ARR ) 
	{
		*p = '[';
		p++;

		temp = json->child;
		while( temp )
		{
			p = json_stringify_token( temp, p );
			if( NULL == p ) return NULL;

			*p = ',';
			p++;

			temp = temp->next;
		}
		p--;
		
		*p = ']';
		p++;
		return p;
	} 
	else if ( json->node_type == JSON_NUM ) 
	{
		memset( str, 0, sizeof(str) );
		sprintf( str, "%.2f\n", json->num_f );
		memcpy( p, str, strlen(str) );
		p += strlen(str);
		return p;
	}
	return NULL;
}


status json_encode( ljson_ctx_t * ctx, meta_t ** string )
{
	uint32 len;
	ljson_node_t * root;
	meta_t * meta;

	root = &ctx->root;
	json_get_child( root, 1, &root );
	len = json_stringify_token_len( root );
	if( len == 0 ) 
	{
		return ERROR;
	}
	if( OK != meta_page_alloc( ctx->page, len, &meta ) ) {
		return ERROR;
	}
	meta->last = (unsigned char *)json_stringify_token( root, (char*)meta->pos );
	if( !meta->last ) 
	{
		return ERROR;
	} 
	else if( meta->last != meta->end )
	{
		return ERROR;
	}
	*string = meta;
	return OK;
}

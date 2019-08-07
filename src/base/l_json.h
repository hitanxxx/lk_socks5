#ifndef _L_JSON_H_INCLUDED_
#define _L_JSON_H_INCLUDED_

#define JSON_OBJ 	0x0001
#define JSON_ARR	0x0002
#define JSON_STR	0x0003
#define JSON_NUM	0x0004
#define JSON_TRUE	0x0005
#define JSON_FALSE	0x0006
#define JSON_NULL	0x0007

#define JSON_ROOT	0xffff

typedef struct ljson_node ljson_node_t;
struct ljson_node 
{
	ljson_node_t * child, *child_last;
	ljson_node_t * next, *parent, *prev;

	uint32		node_type;
	string_t 	name;
	double		num_d;
	int			num_i;
	float		num_f;

} ljson_node;

typedef struct ljson_ctx {
	char * 			p;
	char * 			end;
	ljson_node_t 	root;
	l_mem_page_t * 	page;
} ljson_ctx_t;

// get
status json_get_child( ljson_node_t * parent, uint32 index, ljson_node_t ** child );
status json_get_obj_str( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_bool( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_num( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_null( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_arr( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_obj( ljson_node_t * obj, char * str, uint32 length, ljson_node_t ** child );
status json_get_obj_child_by_name( ljson_node_t * parent, char * str, uint32 length, ljson_node_t ** child );
// add
ljson_node_t * json_add_obj( ljson_ctx_t * ctx, ljson_node_t * parent );
ljson_node_t * json_add_arr( ljson_ctx_t * ctx, ljson_node_t * parent );
ljson_node_t * json_add_true( ljson_ctx_t * ctx, ljson_node_t * parent );
ljson_node_t * json_add_false( ljson_ctx_t * ctx, ljson_node_t * parent );
ljson_node_t * json_add_null( ljson_ctx_t * ctx, ljson_node_t * parent );
ljson_node_t * json_add_str( ljson_ctx_t * ctx, ljson_node_t * parent, char* str, uint32 length );
ljson_node_t * json_add_int( ljson_ctx_t * ctx, ljson_node_t * parent, int32 num );
ljson_node_t * json_obj_add_child( ljson_ctx_t * ctx, ljson_node_t * parent, char * str, uint32 length );

status json_ctx_create( ljson_ctx_t ** json_ctx );
status json_ctx_free( ljson_ctx_t * ctx );
status json_decode( ljson_ctx_t * ctx, char * p, char * end );
status json_encode( ljson_ctx_t * ctx, meta_t ** string );

#endif

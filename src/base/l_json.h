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

typedef struct l_json_node_t json_node_t;
typedef struct l_json_node_t {
	queue_t 	queue;
	queue_t 	child;
	json_node_t *parent;
	uint32		child_flag;

	uint32 		type;
	string_t 	name;
	double 		num;
} l_json_node_t;

typedef struct json_ctx_t {
	char * 			p;
	char * 			end;
	json_node_t 	root;
	l_mem_page_t * 	page;
} json_ctx_t;

// get
status json_get_child( json_node_t * parent, uint32 index, json_node_t ** child );
status json_get_obj_str( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_bool( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_num( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_null( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_arr( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_obj( json_node_t * obj, char * str, uint32 length, json_node_t ** child );
status json_get_obj_child_by_name( json_node_t * parent, char * str, uint32 length, json_node_t ** child );
// add
json_node_t * json_add_obj( json_ctx_t * ctx, json_node_t * parent );
json_node_t * json_add_arr( json_ctx_t * ctx, json_node_t * parent );
json_node_t * json_add_true( json_ctx_t * ctx, json_node_t * parent );
json_node_t * json_add_false( json_ctx_t * ctx, json_node_t * parent );
json_node_t * json_add_null( json_ctx_t * ctx, json_node_t * parent );
json_node_t * json_add_str( json_ctx_t * ctx, json_node_t * parent, char* str, uint32 length );
json_node_t * json_add_num( json_ctx_t * ctx, json_node_t * parent, uint32 num );
json_node_t * json_obj_add_child( json_ctx_t * ctx, json_node_t * parent, char * str, uint32 length );

status json_ctx_create( json_ctx_t ** json_ctx );
status json_ctx_free( json_ctx_t * ctx );
status json_decode( json_ctx_t * ctx, char * p, char * end );
status json_encode( json_ctx_t * ctx, meta_t ** string );

#endif

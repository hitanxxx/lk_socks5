#ifndef _AHO_CORASICK_H_INCLUDED_
#define _AHO_CORASICK_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define EZAC_MAX  128
typedef struct ezac_node_t ezac_node_t;
struct ezac_node_t {
    ezac_node_t * arr[EZAC_MAX];
    ezac_node_t * pfail;
    char fendup:1;
};
typedef struct {    
    ezac_node_t root;
    ezac_node_t * cur;
} ezac_ctx_t;


int ezac_free(ezac_ctx_t * ctx);
ezac_ctx_t * ezac_init();
int ezac_add(ezac_ctx_t * ctx, char * data, int datan);
int ezac_compiler(ezac_ctx_t * ctx);
int ezac_reset(ezac_ctx_t * ctx);
int ezac_find(ezac_ctx_t * ctx, char * data, int datan);


#ifdef __cplusplus
}
#endif

#endif



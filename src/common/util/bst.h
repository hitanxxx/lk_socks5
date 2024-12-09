#ifndef _BST_H_INCLUDED_
#define _BST_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define    BST_STACK_LENGTH        8192
#define BST_LEFT                1
#define BST_RIGHT                2

typedef struct bst_node_t bst_node_t;
typedef status (*bst_travesal_handler)(bst_node_t * node);
struct bst_node_t {
    long long num;
    bst_node_t *parent, *left, *right;
    int level;
    int    type;
};

typedef struct bst_t {
    bst_node_t        head;
    int            elem_num;
} bst_t;

int bst_create(bst_t ** bst);
int bst_free(bst_t * bst);
int bst_add(bst_t * bst, long long key);
bst_node_t * bst_min(bst_node_t * n);
bst_node_t * bst_find(bst_t * bst, long long key);
int bst_del(bst_t * bst, long long key);



#ifdef __cplusplus
}
#endif
        
#endif

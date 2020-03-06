#ifndef _L_BST_H_INCLUDED_
#define _L_BST_H_INCLUDED_

#define	BST_STACK_LENGTH		8192
#define BST_LEFT		1
#define BST_RIGHT		2

typedef struct bst_node_t bst_node_t;
typedef status (*bst_travesal_handler)( bst_node_t * node );
struct bst_node_t {
	int64_t		num;
	bst_node_t *parent, *left, *right;
	uint32				level;
	uint32				type;
};

typedef struct bst_t {
	bst_node_t		head;
	uint32			elem_num;
	bst_travesal_handler handler;
} bst_t;

status bst_create( bst_t * tree, bst_travesal_handler handler );

status bst_del( bst_t * tree, bst_node_t * node );
status bst_insert( bst_t * tree, bst_node_t * insert );
status bst_min( bst_t * tree, bst_node_t ** find );
status bst_reversal( bst_t * tree );

status bst_travesal_breadth( bst_t * tree );
status bst_travesal_deepth_preorder( bst_t * tree );
status bst_travesal_deepth_inorder( bst_t * tree );
status bst_travesal_deepth_postorder( bst_t * tree );

#endif

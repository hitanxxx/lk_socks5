#include "common.h"

static status bst_travesal_print( bst_node_t * node );

// bst_create ----------------------
status bst_create( bst_t * tree, bst_travesal_handler handler )
{
	memset( &tree->head, 0, sizeof(bst_node_t) );
	tree->head.num = -1;
	tree->elem_num = 0;
	( handler ) ? ( tree->handler = handler ) :
		( tree->handler = bst_travesal_print );
	return OK;
}
// bst_branch_min ---------------------
static status bst_branch_min( bst_node_t * node, bst_node_t ** min )
{
	assert( NULL != min );
	while( node->left ) {
		node = node->left;
	}
	*min = node;
	return OK;
}
// bst_find -----------------
static status bst_find( bst_t * tree, bst_node_t * find )
{
	bst_node_t  *stack[BST_STACK_LENGTH];
	uint32 n = 0;
	bst_node_t * node;

	if( !tree->head.right ) {
		return ERROR;
	}
	stack[n++] = tree->head.right;
	while( n ) {
		node = stack[--n];
		if ( node == find ) {
			return OK;
		}
		if( node->left ) {
			stack[n++] = node->left;
		}
		if( node->right ) {
			stack[n++] = node->right;
		}
	}
	return ERROR;
}
// bst_insert ---------------
status bst_insert( bst_t * tree, bst_node_t * insert )
{
	bst_node_t * current;

	assert( NULL != tree );
	assert( NULL != insert );
	if( OK == bst_find( tree, insert ) ) {
		return ERROR;
	}
	insert->parent = NULL;
	insert->left = NULL;
	insert->right = NULL;
	insert->level = 0;
	insert->type = 0;
	current = &tree->head;
	while( 1 ) {
		if( insert->num <= current->num ) {
			if( current->left ) {
				current = current->left;
				continue;
			}
			current->left = insert;
			insert->parent = current;
			insert->level = current->level + 1;
			insert->type = BST_LEFT;
			tree->elem_num ++;
			return OK;
		} else {
			if( current->right ) {
				current = current->right;
				continue;
			}
			current->right = insert;
			insert->parent = current;
			insert->level = current->level + 1;
			insert->type = BST_RIGHT;
			tree->elem_num ++;
			return OK;
		}
	}
}
// bst_del -----------------------
status bst_del( bst_t * tree, bst_node_t * node )
{
	bst_node_t * del, *min;

	assert( NULL != tree );
	assert( NULL != node );
	if( node == &tree->head ) {
		return ERROR;
	}
	if( ERROR == bst_find( tree, node ) ) {
		return ERROR;
	}
	del = node;
	if( !del->left && !del->right ) {
		( del->type == BST_LEFT ) ? ( del->parent->left = NULL ):
			( del->parent->right = NULL );
	} else if ( !del->left && del->right ) {
		( del->type == BST_LEFT ) ? ( del->parent->left = del->right ):
			( del->parent->right = del->right );
		del->right->parent = del->parent;
		del->right->type = del->type;
		del->right->level = del->level;
	} else if ( del->left && !del->right ) {
		( del->type == BST_LEFT ) ? ( del->parent->left = del->left ):
			( del->parent->right = del->left );
		del->left->parent = del->parent;
		del->left->type = del->type;
		del->left->level = del->level;
	} else if ( del->left && del->right ) {
		if( OK != bst_branch_min( del->right, &min ) ) {
			return ERROR;
		}
		/*
			the mininum don't have left child,
			but mininum's type can be BST_LEFT or BST_RIGHT
		*/
		if( min->right ) {
			( min->type == BST_LEFT ) ? ( min->parent->left = min->right ):
				( min->parent->right = min->right );
			min->right->parent = min->parent;
			min->right->type = min->type;
			min->right->level = min->level;
		} else {
			( min->type == BST_LEFT ) ? ( min->parent->left = NULL ):
				( min->parent->right = NULL );
		}
		min->parent = del->parent;
		min->left = del->left;
		min->right = del->right;
		min->level = del->level;
		min->type = del->type;
		if( del->left ) {
			del->left->parent = min;
		}
		if( del->right ) {
			del->right->parent = min;
		}
		( del->type == BST_LEFT ) ? ( del->parent->left = min ):
			( del->parent->right = min );
	}
	del->parent = NULL;
	del->left = NULL;
	del->right = NULL;
	del->level = 0;
	del->type = 0;
	tree->elem_num --;
	return OK;
}
// bst_min ------------------------
status bst_min( bst_t * tree, bst_node_t ** min )
{
	assert( NULL != tree );
	assert( NULL != min );
	if( !tree->head.right ) {
		return ERROR;
	}
	return bst_branch_min( tree->head.right, min );
}
// bst_reversal ------------------
status bst_reversal( bst_t * tree )
{
	bst_node_t * stack[BST_STACK_LENGTH];
	bst_node_t  *temp, *node;
	uint32 n = 0;

	if( !tree->head.right ) {
		return OK;
	}
	stack[n++] = tree->head.right;
	while( n ) {
		node = stack[--n];
		if( node->left && !node->right ) {
			node->right = node->left;
			node->left = NULL;
			stack[n++] = node->right;
		} else if( node->right && !node->left ) {
			node->left = node->right;
			node->right = NULL;
			stack[n++] = node->left;
		} else if( node->left && node->right ) {
			temp = node->left;
			node->left = node->right;
			node->right = temp;
			stack[n++] = node->left;
			stack[n++] = node->right;
		}
	}
	return OK;
}
// bst_travesal_print ------
static status bst_travesal_print( bst_node_t * node )
{
	char str[BST_STACK_LENGTH+1], *ptr;
	uint32 level;

	ptr = str;
	level = node->level;
	while( level ) {
		*ptr++ = '+';
		level --;
	}
	*ptr = '\0';
	debug("%s %ld\n", str, node->num );
	return OK;
}
// bst_travesal_breadth ---------
status bst_travesal_breadth( bst_t * tree )
{
	bst_node_t *stack[2][BST_STACK_LENGTH], *node;
	uint32 current_level_num, next_level_num, index, i;

	if( !tree->head.right ) {
		return OK;
	}
	index = current_level_num = next_level_num = 0;
	stack[index%2][current_level_num++] = tree->head.right;
	while( current_level_num ) {
		index++;
		next_level_num = 0;
		// travesal current level, take lchild rchild to next level
		for( i = 0; i < current_level_num; i ++ ) {
			node = stack[(index-1)%2][i];
			tree->handler( node );
			if( node->left ) {
				stack[index%2][next_level_num++] = node->left;
				if( next_level_num > BST_STACK_LENGTH ) return ERROR;
			}
			if( node->right ) {
				stack[index%2][next_level_num++] = node->right;
				if( next_level_num > BST_STACK_LENGTH ) return ERROR;
			}
		}
		current_level_num = next_level_num;
	}
	return OK;
}
// bst_travesal_deepth_preorder ------
status bst_travesal_deepth_preorder( bst_t * tree )
{
	bst_node_t * node, *stack[BST_STACK_LENGTH];
	uint32 n = 0;

	if( !tree->head.right ) {
		return OK;
	}
	stack[n++] = tree->head.right;
	while( n ) {
		node = stack[--n];
		tree->handler( node );
		if( node->right ) {
			stack[n++] = node->right;
			if( n > BST_STACK_LENGTH ) return ERROR;
		}
		if( node->left ) {
			stack[n++] = node->left;
			if( n > BST_STACK_LENGTH ) return ERROR;
		}
	}
	return OK;
}
// bst_travesal_deepth_inorder ------
status bst_travesal_deepth_inorder( bst_t * tree )
{
	bst_node_t * node, *stack[BST_STACK_LENGTH];
	uint32 n = 0;

	if( !tree->head.right ) {
		return OK;
	}
	node = tree->head.right;
	while( node ) {
		stack[n++] = node;
		if( n > BST_STACK_LENGTH ) return ERROR;
		node = node->left;
	}
	while( n ) {
		node = stack[--n];
		tree->handler( node );
		if( node->right ) {
			node = node->right;
			while( node ) {
				stack[n++] = node;
				if( n > BST_STACK_LENGTH ) return ERROR;
				node = node->left;
			}
		}
	}
	return OK;
}
// bst_travesal_deepth_postorder ---------
status bst_travesal_deepth_postorder( bst_t * tree )
{
	bst_node_t * stack[BST_STACK_LENGTH], *node, *prev = NULL;
	uint32 n = 0;

	if( !tree->head.right ) {
		return OK;
	}
	node = tree->head.right;
	while( node ) {
		stack[n++] = node;
		if( n > BST_STACK_LENGTH ) return ERROR;
		if( node->right ) {
			stack[n++] = node->right;
			if( n > BST_STACK_LENGTH ) return ERROR;
		}
		node = node->left;
	}
	while( n ) {
		node = stack[n-1];
		if( node->right && prev != node->right ) {
			stack[n++] = node->right;
			if( n > BST_STACK_LENGTH ) return ERROR;
			node = node->left;
			while( node ) {
				stack[n++] = node;
				if( n > BST_STACK_LENGTH ) return ERROR;
				if( node->right ) {
					stack[n++] = node->right;
					if( n > BST_STACK_LENGTH ) return ERROR;
				}
				node = node->left;
			}
		}
		node = stack[--n];
		prev = node;
		tree->handler( node );
	}
	return OK;
}

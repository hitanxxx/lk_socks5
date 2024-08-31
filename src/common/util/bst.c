#include "common.h"

int bst_create(bst_t ** bst)
{
    bst_t * nbst = sys_alloc(sizeof(bst_t));
    if(!nbst) {
        err("bst alloc err\n");
        return -1;
    }
    nbst->head.num = -1;
    nbst->head.level = 0;
    *bst = nbst;
    return 0;
}

int bst_free(bst_t * bst)
{
    bst_node_t * stack[8192] = {NULL};
    int stackn = 0;

    if(!bst->head.right) return 0;
    stack[stackn++] = bst->head.right;

    while(stackn > 0) {
        bst_node_t * n = stack[--stackn];
        if(n->left)
            stack[stackn++] = n->left;
        if(n->right)
            stack[stackn++] = n->right;
        sys_free(n);
    }
    return 0;
}

int bst_add(bst_t * bst, long long key)
{
    bst_node_t * n = &bst->head;
    bst_node_t * p = NULL;

    while(n) {
        p = n;
        if(key > n->num) {
            n = n->right;
        } else {
            n = n->left;
        }
    }

    bst_node_t * v = sys_alloc(sizeof(bst_node_t));
    if(!v) {
        err("alloc bst node err\n");
        return -1;
    }
        
    if(key > p->num) {
        p->right = v;
        v->type = BST_RIGHT;
    } else {
        p->left = v;
        v->type = BST_LEFT;
    }
    v->num = key;
    v->level = p->level + 1;
    v->parent = p;
    bst->elem_num ++;
    return 0;
}


bst_node_t * bst_min(bst_node_t * n)
{
    while(n->left) n = n->left;
    return n;
}

bst_node_t * bst_find(bst_t * bst, long long key)
{
    bst_node_t * stack[8192] = {NULL};
    int stackn = 0;

    stack[stackn++] = &bst->head;

    while(stackn > 0) {
        bst_node_t * n = stack[--stackn];
        if(n->num == key) return n;
        if(n->left)
            stack[stackn++] = n->left;
        if(n->right)
            stack[stackn++] = n->right;
    }
    return NULL;
}

int bst_del(bst_t * bst, long long key)
{
    bst_node_t * n = bst_find(bst, key);
    if(!n) 
        return -1;

    if(n->left && !n->right) {
        if(n->type == BST_LEFT) 
            n->parent->left = n->left;
        else 
            n->parent->right = n->left;

        n->left->parent = n->parent;
        n->left->level = n->level;
    } else if (!n->left && n->right) {
        if(n->type == BST_LEFT) 
            n->parent->left = n->right;
        else
            n->parent->right = n->right;

        n->right->parent = n->parent;
        n->right->level = n->level;
    } else {
        ///find right tree mininum node.
        bst_node_t * m = bst_min(n->right);
        ///delete the node
        if(m->right) {
            if(m->type == BST_LEFT)
                m->parent->left = m->right;
            else
                m->parent->right = m->right;
            m->right->parent = m->parent;
            m->right->level = m->level;
        } else {
            m->parent->left = NULL;
        }
        ///replace delete node with this node
        if(n->type == BST_LEFT) 
            n->parent->left = m;
        else
            n->parent->right = m;
        n->left->parent = m;
        n->right->parent = m;
        m->parent = n->parent;
        m->level = n->level;
        m->left = n->left;
        m->right = n->right;
    }
    sys_free(n);
    bst->elem_num --;
    return 0;
}



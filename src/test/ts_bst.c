#include "common.h"
#include "test_main.h"

bst_t * bst = NULL;

static void test_bst_create(void)
{
    t_assert(0 == bst_create(&bst));
}

static void test_bst_add(void)
{
    t_assert(0 == bst_add(bst, 4));
    t_assert(0 == bst_add(bst, 3));
    t_assert(0 == bst_add(bst, 8));
    t_assert(0 == bst_add(bst, 7));
    t_assert(0 == bst_add(bst, 15));
    t_assert(0 == bst_add(bst, 10));
    t_assert(0 == bst_add(bst, 12));
}

static void test_bst_find(void)
{
    t_assert(bst_find(bst, 4));
    t_assert(bst_find(bst, 3));
    t_assert(bst_find(bst, 8));
    t_assert(bst_find(bst, 7));
    t_assert(bst_find(bst, 15));
    t_assert(bst_find(bst, 10));
    t_assert(bst_find(bst, 12));

    t_assert(!bst_find(bst, 99));
}

static void test_bst_del(void)
{
    t_assert(0 == bst_del(bst, 8));
    t_assert(!bst_find(bst, 8));

    bst_node_t * n1 = bst->head.right;
    bst_node_t * n2 = n1->left;
    bst_node_t * n3 = n1->right;
    bst_node_t * n4 = n3->left;
    bst_node_t * n5 = n3->right;
    bst_node_t * n6 = n5->left;
    t_assert(n1 && n1->num == 4);
    t_assert(n2 && n2->num == 3);
    t_assert(n3 && n3->num == 10);
    t_assert(n4 && n4->num == 7);
    t_assert(n5 && n5->num == 15);
    t_assert(n6 && n6->num == 12);
}

void ts_bst_init( )
{
    test_add(test_bst_create);
    test_add(test_bst_add);
    test_add(test_bst_find);
    test_add(test_bst_del);
}

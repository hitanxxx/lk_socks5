#include "macro.h"
#include "test_main.h"
#include "mem.h"

static manager_t manager;

static test_init_pt init_t[] = {
    ts_mem_init,
    ts_list_init,
    ts_queue_init,
    ts_bst_init,
    ts_bheap_init,
    ts_dns_init,
    NULL
};


int test_add(test_pt pt)
{
    manager.test[manager.num].pt = pt;
    manager.num ++;
    return 0;
}

int test_run(void)
{
    int i = 0;
    t_echo ("===== test start =====\n");
    for(i = 0; i < manager.num; i++) {
        manager.test[i].pt();
    }
    t_echo("===== test fin =====\n");
    t_echo("unit num: [%d]\n", manager.num);
    return 0;
}

int test_start(void)
{
    int i = 0;
    mem_pool_init();
    for(i = 0; init_t[i]; i++) {
        init_t[i]();
    }
    test_run();
    mem_pool_deinit();
    return 0;
}

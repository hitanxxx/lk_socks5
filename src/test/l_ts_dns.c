#include "l_macro.h"
#include "l_test.h"
#include "l_dns.h"

#if(0)
static l_timer_t g_ts_timer;
static void local_dns_callback(  void * data )
{
	dns_cycle_t * cycle = data;
	if( cycle->dns_status == OK )
	{
		t_echo("dns process success, query [%s] ipaddr %d.%d.%d.%d\n", 
			cycle->query,
			cycle->answer.rdata->data[0],
			cycle->answer.rdata->data[1],
			cycle->answer.rdata->data[2],
			cycle->answer.rdata->data[3]
		);
	}
	else
	{
		t_echo("dns process failed\n");
	}
	l_dns_free( cycle );
}

static void ts_dns_timer_pt( void * data )
{
	dns_cycle_t * cycle = data;

	t_echo("dns timer action\n");
	l_dns_start( cycle );
}
#endif

static void ts_dns_conv_qname( void )
{
    unsigned char host[255] = {0};
    unsigned char dns[255] = {0};
    uint32 len = 0;
    
    strcpy( (char*)host, "www.baidu.com" );
    len = l_dns_request_qname_conv( dns, (unsigned char*)host );
#if(1)
    int j = 0;
    for( j = 0; j < len; j ++ )
    {
        t_echo("[%d]\n", dns[j] );
    }
#endif
    return ;
}

void ts_dns_init( )
{
#if(0)
	int rc = 0;
	dns_cycle_t * cycle = NULL;
	rc = l_dns_create( &cycle );
	t_assert( rc == OK );

	strncpy( cycle->query, "www.baidu.com", sizeof(cycle->query) );
	strncpy( cycle->dns_serv, l_dns_get_serv(), sizeof(cycle->dns_serv) );
	cycle->cb = local_dns_callback;
	cycle->cb_data = cycle;

	memset( &g_ts_timer, 0, sizeof(l_timer_t) );
	timer_set_data( &g_ts_timer, (void*)cycle );
	timer_set_pt( &g_ts_timer, ts_dns_timer_pt );
	timer_add( &g_ts_timer, 1 );
#endif
    
    test_add(ts_dns_conv_qname);
}


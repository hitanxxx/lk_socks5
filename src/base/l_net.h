#ifndef _L_NET_H_INCLUDED_
#define _L_NET_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

//#define MAX_NET_CON	FD_SETSIZE
#define MAX_NET_CON  1024


typedef status ( *net_send_chain ) ( connection_t * c, meta_t * meta );
typedef status ( *net_recv_chain ) ( connection_t * c, meta_t * meta );
typedef ssize_t ( *net_send ) ( connection_t * c, unsigned char * start, uint32 len );
typedef ssize_t ( *net_recv ) ( connection_t * c, unsigned char * start, uint32 len );

typedef struct l_connection_t
{
    queue_t             queue;
    
    l_mem_page_t        *page;
    int32               fd;
    uint32              con_type;	// enum connection_type
    void*               data;
    meta_t*             meta;

    struct sockaddr_in  addr;

    ssl_connection_t*   ssl;
    uint32              ssl_flag;

    event_t*            event;

    net_send_chain      send_chain;
    net_recv_chain      recv_chain;
    net_send            send;
    net_recv            recv;
} l_connection_t;


status l_socket_nonblocking( int32 fd );
status l_socket_reuseaddr( int32 fd );
status l_socket_fastopen( int32 fd );
status l_socket_nodelay(  int32 fd );
status l_socket_nopush( int32 fd );
status l_socket_send_lowat( int32 fd );
status l_socket_check_status( int32 fd );

status l_net_check_ssl( connection_t * c );
status l_net_accept( event_t * event );
status l_net_connect( connection_t * c, struct sockaddr_in * addr, uint32 con_type );

status net_alloc( connection_t ** connection );
status net_free( connection_t * connection );

status net_init( void );
status net_end( void );
    
#ifdef __cplusplus
}
#endif
    
#endif

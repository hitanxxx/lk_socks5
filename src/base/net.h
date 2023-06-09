#ifndef _NET_H_INCLUDED_
#define _NET_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

//#define MAX_NET_CON	FD_SETSIZE
#define MAX_NET_CON  1024

#define L_NET_TIMEOUT 5

typedef status ( *net_send_chain ) ( connection_t * c, meta_t * meta );
typedef status ( *net_recv_chain ) ( connection_t * c, meta_t * meta );
typedef ssize_t ( *net_send ) ( connection_t * c, unsigned char * start, uint32 len );
typedef ssize_t ( *net_recv ) ( connection_t * c, unsigned char * start, uint32 len );

struct net_connection_t
{
    queue_t             queue;
    
    mem_page_t        *page;
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
};


status net_socket_nbio( int32 fd );
status net_socket_resueaddr( int32 fd );
status net_socket_fastopen( int32 fd );
status net_socket_nodelay(  int32 fd );
status net_socket_nopush( int32 fd );
status net_socket_lowat_send( int32 fd );
status net_socket_check_status( int32 fd );
status net_check_ssl_valid( connection_t * c );

status net_accept( event_t * event );
status net_connect( connection_t * c, struct sockaddr_in * addr);

status net_alloc( connection_t ** connection );
status net_free( connection_t * connection );

void net_timeout( void * data );
status net_init( void );
status net_end( void );
    
#ifdef __cplusplus
}
#endif
    
#endif

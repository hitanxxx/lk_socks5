#ifndef _NET_H_INCLUDED_
#define _NET_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

//#define MAX_NET_CON	FD_SETSIZE
#define MAX_NET_CON  768

#define L_NET_TIMEOUT 5

typedef status ( *net_cb_rw_chain ) ( con_t * c, meta_t * meta );
typedef ssize_t ( *net_cb_rw ) ( con_t * c, unsigned char * start, uint32 len );

struct net_connection_t
{
    int fd;
    struct sockaddr_in addr;
    void*   data;
    meta_t* meta;
    event_t* event;

    net_cb_rw_chain      send_chain;
    net_cb_rw            send;
    net_cb_rw            recv;

    ssl_con_t*   ssl;
    char fssl:1;
};


status net_socket_nbio( int32 fd );
status net_socket_reuseport( int32 fd );
status net_socket_reuseaddr( int32 fd );
status net_socket_fastopen( int32 fd );
status net_socket_nodelay(  int32 fd );
status net_socket_nopush( int32 fd );
status net_socket_lowat_send( int32 fd );
status net_socket_check_status( int32 fd );
status net_check_ssl_valid( con_t * c );

status net_accept( event_t * event );
status net_connect( con_t * c, struct sockaddr_in * addr);

status net_alloc( con_t ** connection );
status net_free( con_t * connection );

void net_timeout( void * data );
status net_init( void );
status net_end( void );
    
#ifdef __cplusplus
}
#endif
    
#endif

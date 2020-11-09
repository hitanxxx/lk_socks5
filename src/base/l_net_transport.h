#ifndef _L_NET_TRANSPORT_H_INCLUDED_
#define _L_NET_TRANSPORT_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#define NET_TRANSPORT_BUFFER_SIZE  8192
#define NET_TRANSPORT_BUFFER_NUM    10

typedef struct net_transport_t
{
	connection_t*   recv_connection;
	connection_t*   send_connection;
	meta_t *  recv_meta;
	uint32    recv_time;
	uint32    recv_error;
	meta_t *  send_meta;

	uint32	cache_size;
} net_transport_t;

status net_transport_alloc( net_transport_t ** t );
status net_transport_free( net_transport_t * t );
status net_transport( net_transport_t * t, uint32 write );

#ifdef __cplusplus
}
#endif
    
#endif

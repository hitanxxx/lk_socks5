#include "lk.h"

// net_transport_send_necessary ----------
static status net_transport_send_necessary( net_transport_t * t )
{
	if( !t->send_meta ) {
		t->recv_meta = NULL;
		return ERROR;
	}
	if( t->send_meta == t->recv_meta ) {
		if( t->send_meta->pos < t->send_meta->last ) {
			return OK;
		} else {
			return ERROR;
		}
	}
	return OK;
}
// net_transport_send ---------
static status net_transport_send( net_transport_t * t )
{
	ssize_t rc;
	meta_t * next;

	while( 1 ) {
		if( OK != net_transport_send_necessary( t ) ) {
			if( t->recv_error == 1 ) {
				return ERROR;
			}
			return AGAIN;
		}
		rc = t->send_connection->send( t->send_connection,
			t->send_meta->pos,
			meta_len( t->send_meta->pos, t->send_meta->last ) );
		if( rc == ERROR ) {
			return ERROR;
		} else if ( rc == AGAIN ) {
			return AGAIN;
		} else {
			t->send_meta->pos += rc;
		}
		if( t->send_meta->pos == t->send_meta->last ) {
			next = t->send_meta->next;
			meta_free( t->send_meta );
			t->send_meta = next;
			t->recv_time --;
		}
	}
}
// net_transport_recv ----------
static status net_transport_recv( net_transport_t * t )
{
	meta_t * new;
	ssize_t rc;

	if( t->recv_error ) {
		return OK;
	}

	while( 1 ) {
		if( !t->recv_meta ||
			t->recv_meta->last == t->recv_meta->end ) {
			if( OK != meta_alloc( &new, NET_TRANSPORT_BUFFER_SIZE ) ) {
				err_log(  "%s --- l_safe_malloc new", __func__ );
				return ERROR;
			}
			t->recv_time ++;
			if( NULL == t->recv_meta ) {
				t->recv_meta = new;
				t->send_meta = t->recv_meta;
			} else {
				t->recv_meta->next = new;
				t->recv_meta = t->recv_meta->next;
			}
		}
		rc = t->recv_connection->recv( t->recv_connection,
			t->recv_meta->last,
			meta_len( t->recv_meta->last, t->recv_meta->end ) );
		if( rc == ERROR ) {
			return ERROR;
		} else if ( rc == AGAIN ) {
			return AGAIN;
		} else {
			t->recv_meta->last += rc;
		}
	}
}
// net_transport -----------------
status net_transport( net_transport_t * t, uint32 write )
{
	status rc;

	while( 1 ) {
		if( write ) {
			rc = net_transport_send( t );
			if( rc == ERROR ) {
				return ERROR;
			}
			return rc;
		}
		rc = net_transport_recv( t );
		if( rc == ERROR ) {
			t->recv_error = 1;
		}
		write = 1;
	}
}
// net_transport_free -------------
status net_transport_free( net_transport_t * t )
{
	meta_t * meta;

	while( t->send_meta ) {
		meta = t->send_meta->next;
		meta_free( t->send_meta );
		t->send_meta = meta;
	}
	l_safe_free( t );
	return OK;
}
// net_transport_alloc ------------
status net_transport_alloc( net_transport_t ** t )
{
	net_transport_t * new = NULL;

	new = (net_transport_t*)l_safe_malloc( sizeof(net_transport_t) );
	if( !new ) {
		err_log(  "%s --- l_safe_malloc new", __func__ );
		return ERROR;
	}
	memset( new, 0, sizeof(net_transport_t) );
	*t = new;
	return OK;
}

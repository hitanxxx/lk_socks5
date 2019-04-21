#include "lk.h"

// send_chains_iovec ---------
static status send_chains_iovec( send_iovec_t * send_iov, meta_t * meta )
{
	meta_t *cl;
	int32 n = 0;

	send_iov->all_len = 0;
	send_iov->iov_count = 0;
	for( cl = meta; cl != NULL; cl = cl->next ) {
		if( cl->file_flag ) {
			break;
		}
		if( n > MAX_IVO_NUM ) {
			err ( " meta chain > MAX_IVO_NUM\n" );
			return ERROR;
		}
		send_iov->all_len += meta_len( cl->pos, cl->last );
		send_iov->iovec[n].iov_base = cl->pos;
		send_iov->iovec[n++].iov_len = meta_len( cl->pos, cl->last );
		send_iov->iov_count ++;
	}
	return OK;
}

// send_chains_update ----------------------
static meta_t * send_chains_update( meta_t * meta, ssize_t len )
{
	meta_t *cl;
	uint32 sent;

	sent = (uint32)len;

	for( cl = meta; cl != NULL && sent > 0; cl = cl->next ) {
		if( cl->file_flag ) {
			if( sent >= (cl->file_last - cl->file_pos) ) {
				sent -= (cl->file_last - cl->file_pos);
				cl->file_pos = cl->file_last;
			} else {
				cl->file_pos += (uint32)sent;
				sent = 0;
				break;
			}
		} else {
			if( sent >= meta_len( cl->pos, cl->last ) ) {
				sent -= meta_len( cl->pos, cl->last );
				cl->pos = cl->last;
			} else {
				cl->pos += sent;
				sent = 0;
				break;
			}
		}
	}
	return cl;
}
// send_chains -----------------
status send_chains( connection_t * c , meta_t * send_meta )
{
	ssize_t sent;
	off_t offset;
	send_iovec_t send_iov;
	meta_t * meta, *n;

	meta = send_meta;

	// jump empty
	while( meta ) {
		n = meta->next;
		if( meta->file_flag ) {
			if( meta->file_pos < meta->file_last ) {
				break;
			}
		} else {
			if( meta_len( meta->pos, meta->last ) > 0 ) {
				break;
			}
		}
		meta = n;
	}
	if( !meta ) {
		err ( " meta chain empty\n" );
		return ERROR;
	}

	while( 1 ) {
		sent = send_chains_iovec( &send_iov, meta );
		if( sent != OK ) {
			if( sent == ERROR ) {
				err ( " send_chains_iovec failed\n" );
				return ERROR;
			}
		}

eintr:
		if( meta->file_flag ) {
			return ERROR;
			// fix me!!!
		} else {
			sent = writev( c->fd, send_iov.iovec, (int)send_iov.iov_count );
			if( sent == ERROR ) {
				if( errno == EINTR ) {
					goto eintr;
				} else if ( errno == EAGAIN ) {
					return AGAIN;
				} else {
					err ( " writev failed, [%d]\n", errno );
					return ERROR;
				}
			} else if ( sent == 0 ) {
				err ( " writev return 0, peer closed\n" );
				return ERROR;
			}
		}

		meta = send_chains_update( meta, sent );
		if( !meta ) {
			return DONE;
		}
	}
}
// recvs ----------------------
ssize_t recvs( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;

	rc = recv( c->fd, start, len, 0 );
	if( rc == ERROR ) {
		if( errno == EAGAIN ) {
			return AGAIN;
		}
		err(" recv failed, [%d]\n", errno );
		return ERROR;
	} else if ( rc == 0 ) {
		err ( " recv return 0, peer closed\n" );
		return ERROR;
	} else {
		return rc;
	}
}
// sends ----------------------
ssize_t sends( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;

	rc = send( c->fd, start, len, 0 );
	if( rc < 0 ) {
		if( errno == EAGAIN ) {
			return AGAIN;
		}
		err ( " send failed, [%d]\n", errno );
		return ERROR;
	} else if ( rc == 0 ) {
		err ( " send return 0, peer closed\n" );
		return ERROR;
	} else {
		return rc;
	}
}

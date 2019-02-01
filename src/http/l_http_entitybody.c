#include "lk.h"

static queue_t usable;
static queue_t in_use;
static http_entitybody_t * pool = NULL;

// http_entity_parse_chunk -----------------------------------------------
static status http_entity_parse_chunk( http_entitybody_t * bd )
{
	status rc = AGAIN;
	char * p;
	uint32 recvd;
	int32  length;

	enum {
		chunk_start = 0,
		chunk_hex,
		chunk_hex_rn,
		chunk_part,
		chunk_part_r,
		chunk_part_rn,
		chunk_last_r,
		chunk_last_rn
	} state;
	state = bd->state;
	for( p = bd->chunk_pos; p < bd->body_last->last; p ++ ) {
		if( state == chunk_start ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' )
			) {
				bd->hex_str[bd->hex_length++] = *p;
				state = chunk_hex;
				continue;
			} else {
				err_log(  "%s --- chunk_start illegal, [%d]", __func__, *p );
				return ERROR;
			}
		}
		if ( state == chunk_hex ) {
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ||
			( *p == 'x' ) ||
			( *p == 'X' ) ||
			(*p == '\r' )
			) {
				if( *p == '\r' ) {
					state = chunk_hex_rn;
				} else {
					bd->hex_str[bd->hex_length++] = *p;
				}
				continue;
			} else {
				err_log(  "%s --- chunk_hex illegal, [%d]", __func__, *p );
				return ERROR;
			}
		}
		if ( state == chunk_hex_rn ) {
			if( *p != '\n' ) {
				err_log(  "%s --- chunk_hex_rn illegal, [%d]", __func__, *p );
				return ERROR;
			}
			if( OK != l_hex2dec( bd->hex_str, bd->hex_length, &length ) ) {
				err_log("%s --- chunk partnum hex2dec failed, [%.*s]", __func__,
				bd->hex_length, bd->hex_str );
				return ERROR;
			}
			bd->hex_length = 0;
			bd->chunk_length = (uint32)length;
			if( bd->chunk_length == 0 ) {
				state = chunk_last_r;
				continue;
			} else {
				bd->chunk_recvd = 0;
				bd->chunk_need = bd->chunk_length;
				state = chunk_part;
				continue;
			}
		}
		if ( state == chunk_part ) {
			recvd = meta_len( p, bd->body_last->last );
			if( recvd >= bd->chunk_need ) {
				p += bd->chunk_need;
				state = chunk_part_r;
			} else {
				bd->chunk_recvd += recvd;
				bd->chunk_need -= recvd;
				p = bd->body_last->last;
				state = chunk_part;
				break;
			}
		}
		if( state == chunk_part_r ) {
			if( *p != '\r' ) {
				err_log(  "%s --- chunk_part_r illegal, [%d]", __func__, *p );
				return ERROR;
			}
			state = chunk_part_rn;
			continue;
		}
		if( state == chunk_part_rn ) {
			if( *p != '\n' ) {
				err_log(  "%s --- chunk_part_rn illegal, [%d]", __func__, *p );
				return ERROR;
			}
			state = chunk_start;
			continue;
		}
		if( state == chunk_last_r ) {
			if( *p != '\r' ) {
				err_log(  "%s --- chunk_last_r illegal, [%d]", __func__, *p );
				return ERROR;
			}
			state = chunk_last_rn;
			continue;
		}
		if ( state == chunk_last_rn ) {
			if( *p != '\n' ) {
				err_log(  "%s --- chunk_last_rn illegal, [%d]", __func__, *p );
				return ERROR;
			}
			state = chunk_start;
			p++;
			rc = DONE;
			break;
		}
	}
	bd->chunk_pos = p;
	bd->state = state;
	return rc;
}
// http_entity_process_content --------
static status http_entity_process_content( http_entitybody_t * bd )
{
	meta_t * new;
	ssize_t rc;
	while ( 1 ) {
		if( !bd->cache ) {
			if( bd->body_last->last == bd->body_last->end ) {
				bd->body_last->last = bd->body_last->pos;
			}
		} else {
			if( ( !bd->body_last ) || ( bd->body_last->last == bd->body_last->end ) ) {
				if( OK != meta_alloc( &new, ENTITY_BODY_BUFFER_SIZE ) ) {
					err_log("%s --- alloc meta new", __func__ );
					return ERROR;
				}
				if( !bd->body_last ) {
					bd->body_last = new;
					bd->body_head = bd->body_last;
				} else {
					bd->body_last->next = new;
					bd->body_last = bd->body_last->next;
				}
			}
		}
		rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
		if( rc == ERROR ) {
			err_log("%s --- recv failed, [%d]", __func__, errno );
			return ERROR;
		} else if ( rc == AGAIN ) {
			return AGAIN;
		} else {
			bd->body_last->last += rc;
			bd->content_need -= (uint32)rc;
			if( !bd->content_need ) {
				bd->all_length = bd->content_length;
				return DONE;
			}
		}
	}
}
// http_entity_process_chunk -------
static status http_entity_process_chunk( http_entitybody_t * bd )
{
	ssize_t rc;
	meta_t * new;
	while( 1 ) {
		if( !bd->cache ) {
			if( bd->body_last->last == bd->body_last->end ) {
				bd->body_last->last = bd->body_last->pos;
				bd->chunk_pos = bd->body_last->pos;
			}
		} else {
			if( ( !bd->body_last ) || ( bd->body_last->last == bd->body_last->end )  ) {
				if( OK != meta_alloc( &new, ENTITY_BODY_BUFFER_SIZE ) ) {
					err_log("%s --- alloc meta new", __func__ );
					return ERROR;
				}
				if( !bd->body_last ) {
					bd->body_last = new;
					bd->body_head = bd->body_last;
				} else {
					bd->body_last->next = new;
					bd->body_last = bd->body_last->next;
				}
				bd->chunk_pos = bd->body_last->pos;
			}
		}
		if( bd->chunk_pos == bd->body_last->last ) {
			rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
			if( rc == ERROR ) {
				err_log("%s --- recv failed, [%d]", __func__, errno );
				return ERROR;
			} else if ( rc == AGAIN ) {
				return AGAIN;
			} else {
				bd->body_last->last += rc;
			}
		}
		rc = http_entity_parse_chunk( bd );
		if( rc == DONE ) {
			return DONE;
		} else if ( rc == ERROR ) {
			err_log("%s --- parse chunk failed", __func__ );
			return ERROR;
		}
	}
}
// http_entitybody_start ------
static status http_entitybody_start( http_entitybody_t * bd )
{
	uint32 busy_head = 0;
	meta_t * new;

	busy_head = meta_len( bd->c->meta->pos, bd->c->meta->last );
	if( bd->body_type == HTTP_ENTITYBODY_CONTENT ) {
		bd->content_need = bd->content_length;
		if( busy_head == bd->content_need ) {
			// if busy contain all body
			bd->content_need -= busy_head;
			bd->all_length = bd->content_length;
			if( !bd->cache ) {
				return DONE;
			} else {
				if( OK != meta_alloc( &bd->body_head, busy_head ) ) {
					err_log("%s --- alloc body head", __func__ );
					return ERROR;
				}
				bd->body_last = bd->body_head;
				l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
				bd->body_last->last += busy_head;
				return DONE;
			}
		} else if ( busy_head < bd->content_need ) {
			if( !bd->cache ) {
				if( OK != meta_alloc( &bd->body_head, ENTITY_BODY_BUFFER_SIZE ) ) {
					err_log("%s --- alloc body head", __func__ );
					return ERROR;
				}
				bd->body_last = bd->body_head;
			} else {
				if( OK != meta_alloc( &bd->body_head, busy_head ) ) {
					err_log("%s --- alloc body head", __func__ );
					return ERROR;
				}
				bd->body_last = bd->body_head;
				l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
				bd->body_last->last += busy_head;
			}
			bd->content_need -= busy_head;
			bd->handler = http_entity_process_content;
			return bd->handler( bd );
		} else {
			err_log("%s --- busy head more than content length...", __func__ );
			return ERROR;
		}
	} else if( bd->body_type == HTTP_ENTITYBODY_CHUNK ) {
		// make frist meta not full
		if( busy_head ) {
			if( OK != meta_alloc( &bd->body_head, busy_head + ENTITY_BODY_BUFFER_SIZE ) ) {
				err_log("%s --- alloc body head", __func__ );
				return ERROR;
			}
			bd->body_last = bd->body_head;

			l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
			bd->body_last->last += busy_head;
		} else {
			if( OK != meta_alloc( &bd->body_head, ENTITY_BODY_BUFFER_SIZE ) ) {
				err_log("%s --- alloc body head", __func__ );
				return ERROR;
			}
			bd->body_last = bd->body_head;
		}
		bd->chunk_pos = bd->body_last->pos;
		bd->handler = http_entity_process_chunk;
		return bd->handler( bd );
	}
	err_log("%s --- not support entity body type", __func__ );
	return ERROR;
}
//http_entitybody_alloc -----------
static status http_entitybody_alloc( http_entitybody_t ** body )
{
	http_entitybody_t * new;
	queue_t * q;

	if( queue_empty( &usable ) == 1 ) {
		err_log(  "%s --- queue usabel empty", __func__ );
		return ERROR;
	}
	q = queue_head( &usable );
	queue_remove( q );
	queue_insert( &in_use, q );
	new = l_get_struct( q, http_entitybody_t, queue );
	*body = new;
	return OK;
}
// http_entitybody_create ------------------------------
status http_entitybody_create( connection_t * c, http_entitybody_t ** body )
{
	http_entitybody_t * new;

	if( OK != http_entitybody_alloc( &new ) ) {
		err_log(  "%s --- alloc new", __func__ );
		return ERROR;
	}
	new->c = c;
	new->handler = http_entitybody_start;
	new->state = 0;
	*body = new;
	return OK;
}
// http_entitybody_free -------------------
status http_entitybody_free( http_entitybody_t * bd )
{
	meta_t *t, *n;
	queue_remove( &bd->queue );
	queue_insert_tail( &usable, &bd->queue );

	bd->c = NULL;
	bd->handler = NULL;
	bd->state = 0;

	bd->body_type = HTTP_ENTITYBODY_NULL;
	bd->cache = 0;

	bd->content_length = 0;
	bd->content_need = 0;
	//bd->content_end = NULL;

	bd->chunk_pos = NULL;
	bd->chunk_length = 0;
	bd->chunk_recvd = 0;
	bd->chunk_need = 0;
	bd->chunk_all_length = 0;

	// clear hex_str ?
	bd->hex_length = 0;

	if( bd->body_head ) {
		t = bd->body_head;
		while(t) {
			n = t->next;
			meta_free( t );
			t = n;
		}
	}
	bd->body_head = NULL;
	bd->body_last = NULL;
	return OK;
}
// http_entitybody_init_module ------------
status http_entitybody_init_module( void )
{
	uint32 i;

	queue_init( &usable );
	queue_init( &in_use );
	pool = (http_entitybody_t*)l_safe_malloc( sizeof(http_entitybody_t)*MAXCON );
	if( !pool ) {
		err_log(  "%s --- l_safe_malloc pool", __func__ );
		return ERROR;
	}
	memset( pool, 0, sizeof(http_entitybody_t)*MAXCON );
	for( i = 0; i < MAXCON; i ++ ) {
		queue_insert_tail( &usable, &pool[i].queue );
	}
	return OK;
}
// http_entitybody_end_module -----------
status http_entitybody_end_module( void )
{
	if( pool ) {
		l_safe_free( pool );
		pool = NULL;
	}
	return OK;
}

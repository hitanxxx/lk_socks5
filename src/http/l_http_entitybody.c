#include "l_base.h"
#include "l_http_entitybody.h"

static queue_t g_queue_usable;
static queue_t g_queue_use;
static http_entitybody_t * g_pool = NULL;

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
	for( p = bd->chunk_pos; p < bd->body_last->last; p ++ )
	{
		if( state == chunk_start ) 
		{
			if(( *p >= '0' && *p <= '9' ) ||( *p >= 'a' && *p <= 'f' ) ||( *p >= 'A' && *p <= 'F' )) 
			{
				bd->hex_str[bd->hex_length++] = *p;
				state = chunk_hex;
				continue;
			} 
			else 
			{
				err(  " chunk_start illegal, [%d]\n", *p );
				return ERROR;
			}
		}
		if ( state == chunk_hex ) 
		{
			if(
			( *p >= '0' && *p <= '9' ) ||
			( *p >= 'a' && *p <= 'f' ) ||
			( *p >= 'A' && *p <= 'F' ) ||
			( *p == 'x' ) ||
			( *p == 'X' ) ||
			(*p == '\r' )
			) 
			{
				if( *p == '\r' ) 
				{
					state = chunk_hex_rn;
				} 
				else 
				{
					bd->hex_str[bd->hex_length++] = *p;
				}
				continue;
			} 
			else 
			{
				err(  "chunk_hex illegal, [%d]\n", *p );
				return ERROR;
			}
		}
		if ( state == chunk_hex_rn ) 
		{
			if( *p != '\n' ) 
			{
				err( " chunk_hex_rn illegal, [%d]\n", *p );
				return ERROR;
			}
			if( OK != l_hex2dec( bd->hex_str, bd->hex_length, &length ) ) 
			{
				err(" chunk partnum hex2dec failed, [%.*s]\n",bd->hex_length, bd->hex_str );
				return ERROR;
			}
			bd->hex_length = 0;
			bd->chunk_length = (uint32)length;
			if( bd->chunk_length == 0 )
			{
				state = chunk_last_r;
				continue;
			} 
			else
			{
				bd->chunk_recvd = 0;
				bd->chunk_need = bd->chunk_length;
				state = chunk_part;
				continue;
			}
		}
		if ( state == chunk_part )
		{
			recvd = meta_len( p, bd->body_last->last );
			if( recvd >= bd->chunk_need ) 
			{
				p += bd->chunk_need;
				state = chunk_part_r;
			}
			else 
			{
				bd->chunk_recvd += recvd;
				bd->chunk_need -= recvd;
				p = bd->body_last->last;
				state = chunk_part;
				break;
			}
		}
		if( state == chunk_part_r ) 
		{
			if( *p != '\r' ) 
			{
				err( "chunk_part_r illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_part_rn;
			continue;
		}
		if( state == chunk_part_rn )
		{
			if( *p != '\n' ) 
			{
				err( " chunk_part_rn illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_start;
			continue;
		}
		if( state == chunk_last_r ) 
		{
			if( *p != '\r' ) 
			{
				err( " chunk_last_r illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_last_rn;
			continue;
		}
		if ( state == chunk_last_rn ) 
		{
			if( *p != '\n' ) 
			{
				err(  " chunk_last_rn illegal, [%d]\n", *p );
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

static status http_entity_process_content( http_entitybody_t * bd )
{
	meta_t * new;
	ssize_t rc;
	while ( 1 ) 
	{
		if( !bd->cache ) 
		{
			if( bd->body_last->last == bd->body_last->end ) 
			{
				bd->body_last->last = bd->body_last->pos;
			}
		} 
		else 
		{
			if( ( !bd->body_last ) || ( bd->body_last->last == bd->body_last->end ) ) 
			{
				if( OK != meta_alloc( &new, ENTITY_BODY_BUFFER_SIZE ) )
				{
					err(" alloc meta new\n");
					return ERROR;
				}
				if( !bd->body_last )
				{
					bd->body_last = new;
					bd->body_head = bd->body_last;
				} 
				else
				{
					bd->body_last->next = new;
					bd->body_last = bd->body_last->next;
				}
			}
		}
		rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
		if( rc == ERROR ) 
		{
			err("recv failed, [%d]\n", errno );
			return ERROR;
		} 
		else if ( rc == AGAIN )
		{
			return AGAIN;
		} 
		else 
		{
			bd->body_last->last += rc;
			bd->content_need -= (uint32)rc;
			if( !bd->content_need ) 
			{
				bd->all_length = bd->content_length;
				bd->status = HTTP_BODY_RECVD;
				return DONE;
			}
		}
	}
}

static status http_entity_process_chunk( http_entitybody_t * bd )
{
	ssize_t rc;
	meta_t * new;
	while( 1 ) 
	{
		if( !bd->cache ) 
		{
			if( bd->body_last->last == bd->body_last->end )
			{
				bd->body_last->last = bd->body_last->pos;
				bd->chunk_pos = bd->body_last->pos;
			}
		} 
		else 
		{
			if( ( !bd->body_last ) || ( bd->body_last->last == bd->body_last->end )  ) 
			{
				if( OK != meta_alloc( &new, ENTITY_BODY_BUFFER_SIZE ) ) 
				{
					err(" alloc meta new\n");
					return ERROR;
				}
				if( !bd->body_last ) 
				{
					bd->body_last = new;
					bd->body_head = bd->body_last;
				} 
				else 
				{
					bd->body_last->next = new;
					bd->body_last = bd->body_last->next;
				}
				bd->chunk_pos = bd->body_last->pos;
			}
		}
		
		if( bd->chunk_pos == bd->body_last->last ) 
		{
			rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
			if( rc == ERROR ) 
			{
				err(" recv failed, [%d]\n", errno );
				return ERROR;
			} 
			else if ( rc == AGAIN ) 
			{
				return AGAIN;
			} 
			else 
			{
				bd->body_last->last += rc;
			}
		}
		rc = http_entity_parse_chunk( bd );
		if( rc == DONE ) 
		{
			bd->status = HTTP_BODY_RECVD;
			return DONE;
		} 
		else if ( rc == ERROR ) 
		{
			err(" parse chunk failed\n" );
			return ERROR;
		}
	}
}


static status http_entitybody_start( http_entitybody_t * bd )
{
	uint32 busy_head = 0;
	meta_t * new;

	busy_head = meta_len( bd->c->meta->pos, bd->c->meta->last );
	if( bd->body_type == HTTP_ENTITYBODY_NULL )
	{
		bd->status = HTTP_BODY_EMPTY;
		return DONE;
	}
	else if( bd->body_type == HTTP_ENTITYBODY_CONTENT ) 
	{
		bd->content_need = bd->content_length;
		if( busy_head == bd->content_need ) 
		{
			// if busy contain all body
			bd->content_need -= busy_head;
			bd->all_length = bd->content_length;
			if( !bd->cache ) 
			{
				bd->status = HTTP_BODY_EMPTY;
				return DONE;
			} 
			else 
			{
				if( OK != meta_alloc( &bd->body_head, busy_head ) )
				{
					err(" alloc body head\n" );
					return ERROR;
				}
				bd->body_last = bd->body_head;
				l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
				bd->body_last->last += busy_head;
				
				bd->status = HTTP_BODY_RECVD;
				return DONE;
			}
		} 
		else if ( busy_head < bd->content_need ) 
		{
			if( !bd->cache ) 
			{
				if( OK != meta_alloc( &bd->body_head, ENTITY_BODY_BUFFER_SIZE ) ) 
				{
					err(" alloc body head\n" );
					return ERROR;
				}
				bd->body_last = bd->body_head;
			} 
			else 
			{
				if( OK != meta_alloc( &bd->body_head, busy_head ) ) 
				{
					err("alloc body head\n");
					return ERROR;
				}
				bd->body_last = bd->body_head;
				l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
				bd->body_last->last += busy_head;
			}
			bd->content_need -= busy_head;
			bd->handler.process = http_entity_process_content;
			return bd->handler.process( bd );
		} 
		else 
		{
			err(" busy head more than content length...\n");
			return ERROR;
		}
	} 
	else if( bd->body_type == HTTP_ENTITYBODY_CHUNK ) 
	{
		// make frist meta not full
		if( busy_head ) 
		{
			if( OK != meta_alloc( &bd->body_head, busy_head + ENTITY_BODY_BUFFER_SIZE ) ) 
			{
				err("alloc body head\n" );
				return ERROR;
			}
			bd->body_last = bd->body_head;

			l_memcpy( bd->body_last->last, bd->c->meta->pos, busy_head );
			bd->body_last->last += busy_head;
		} 
		else
		{
			if( OK != meta_alloc( &bd->body_head, ENTITY_BODY_BUFFER_SIZE ) ) 
			{
				err("alloc body head\n" );
				return ERROR;
			}
			bd->body_last = bd->body_head;
		}
		bd->chunk_pos = bd->body_last->pos;
		bd->handler.process = http_entity_process_chunk;
		return bd->handler.process( bd );
	}
	err(" not support entity body type\n" );
	return ERROR;
}

static status http_entitybody_alloc( http_entitybody_t ** body )
{
	http_entitybody_t * new;
	queue_t * q;

	if( queue_empty( &g_queue_usable ) == 1 ) 
	{
		err(  "queue usabel empty" );
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert( &g_queue_use, q );
	new = l_get_struct( q, http_entitybody_t, queue );
	*body = new;
	return OK;
}

static status http_entitybody_free( http_entitybody_t * bd )
{
	meta_t *t, *n;
	queue_remove( &bd->queue );
	queue_insert_tail( &g_queue_usable, &bd->queue );

	bd->c = NULL;
	bd->handler.process = NULL;
	bd->handler.exit = NULL;
	bd->state = 0;

	bd->body_type = HTTP_ENTITYBODY_NULL;
	bd->cache = 0;

	bd->content_length = 0;
	bd->content_need = 0;


	bd->chunk_pos = NULL;
	bd->chunk_length = 0;
	bd->chunk_recvd = 0;
	bd->chunk_need = 0;
	bd->chunk_all_length = 0;

	// clear hex_str ?
	bd->hex_length = 0;

	if( bd->body_head ) 
	{
		t = bd->body_head;
		while(t) 
		{
			n = t->next;
			meta_free( t );
			t = n;
		}
	}
	bd->body_head = NULL;
	bd->body_last = NULL;
	return OK;
}

status http_entitybody_create( connection_t * c, http_entitybody_t ** body, int discard )
{
	http_entitybody_t * local_body;

	if( OK != http_entitybody_alloc( &local_body ) ) 
	{
		err( "alloc local_body" );
		return ERROR;
	}
	local_body->c = c;

	local_body->cache = ((discard == 1) ? 0 : 1 );
	local_body->handler.process = http_entitybody_start;
	local_body->handler.exit	= http_entitybody_free;
	
	local_body->state = 0;
	*body = local_body;
	return OK;
}

status http_entitybody_init_module( void )
{
	uint32 i;

	queue_init( &g_queue_usable );
	queue_init( &g_queue_use );
	g_pool = (http_entitybody_t*)l_safe_malloc( sizeof(http_entitybody_t)*MAXCON );
	if( !g_pool ) 
	{
		err(  " l_safe_malloc g_pool\n" );
		return ERROR;
	}
	memset( g_pool, 0, sizeof(http_entitybody_t)*MAXCON );
	for( i = 0; i < MAXCON; i ++ ) 
	{
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}
	return OK;
}

status http_entitybody_end_module( void )
{
	if( g_pool ) 
	{
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}

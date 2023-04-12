#include "common.h"
#include "http_body.h"

static queue_t g_queue_usable;
static queue_t g_queue_use;
static http_body_t * g_pool = NULL;

static status http_body_parse_chunk( http_body_t * bd )
{
	status rc = AGAIN;
	unsigned char * p = NULL;

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

	/*
		chunk data format
		...
		| data len(hex) |
		| \r\n    		|
		| data  		|
		| \r\n          |
		...
		| data len(0)   |
		| \r\n          |
		  END
	 */
	
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
				err("http body chunk_start illegal, [%d]\n", *p );
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
				err("chunk_hex illegal, [%d]\n", *p );
				return ERROR;
			}
		}
		if ( state == chunk_hex_rn ) 
		{
            int32  length = 0;
			if( *p != '\n' ) 
			{
				err("http body chunk_hex_rn illegal, [%d]\n", *p );
				return ERROR;
			}
            length = strtol( (char*)bd->hex_str, NULL, 16 );
			bd->body_length += length;
			if( length > 0 )
			{
				bd->chunk_part_cur 	= 0;
				bd->chunk_part_len 	= length;
				state = chunk_part;
			}
			else
			{
				state = chunk_last_r;
			}
            memset( bd->hex_str, 0, sizeof(bd->hex_str) );
			bd->hex_length 	= 0;
			continue;
		}
		if ( state == chunk_part )
		{
			uint32 recvd = meta_len( p, bd->body_last->last );
			if( recvd >= bd->chunk_part_len ) 
			{
				p 		+= bd->chunk_part_len;
				state 	= chunk_part_r;
			}
			else 
			{
				bd->chunk_part_cur	+= recvd;
				bd->chunk_part_len 	-= recvd;
				p 		= bd->body_last->last;
				break;
			}
		}
		if( state == chunk_part_r ) 
		{
			if( *p != '\r' ) 
			{
				err("http body chunk_part_r illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_part_rn;
			continue;
		}
		if( state == chunk_part_rn )
		{
			if( *p != '\n' ) 
			{
				err("http body chunk_part_rn illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_start;
			continue;
		}
		if( state == chunk_last_r ) 
		{
			if( *p != '\r' ) 
			{
				err("http body chunk_last_r illegal, [%d]\n", *p );
				return ERROR;
			}
			state = chunk_last_rn;
			continue;
		}
		if ( state == chunk_last_rn ) 
		{
			if( *p != '\n' ) 
			{
				err("http body chunk_last_rn illegal, [%d]\n", *p );
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


static status http_body_process_chunk( http_body_t * bd )
{
	ssize_t rc;
	meta_t * new = NULL;
	while( 1 ) 
	{
		if( meta_len( bd->body_last->last, bd->body_last->end ) <= 0 ) 
		{
			if( OK != meta_alloc_form_mempage( bd->c->page, ENTITY_BODY_BUFFER_SIZE, &new ) ) 
			{
				err("http body chunk alloc append meta failed\n");
				return ERROR;
			}
			bd->body_last->next 	= new;
			bd->body_last 			= new;
			bd->chunk_pos 			= bd->body_last->pos = bd->body_last->last;
		}
	
		if( bd->chunk_pos == bd->body_last->last ) 
		{
			rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
			if( rc < 0 )
			{
				if( rc == ERROR )
				{
					err("http body recv failed\n");
				}
				return (status)rc;
			}
			else 
			{
				bd->body_last->last += rc;
			}
		}

		rc = http_body_parse_chunk( bd );
		if ( rc == ERROR ) 
		{
			err("http body parse chunk failed\n" );
			return ERROR;
		}
		else if( rc == DONE ) 
		{
			bd->body_status |= HTTP_BODY_STAT_DONE_CACHE;
			return DONE;
		}
	}
}

static status http_body_process_content( http_body_t * bd )
{
	meta_t * new = NULL;
	ssize_t rc = 0;
	
	while ( 1 ) 
	{
        if( meta_len( bd->body_last->last, bd->body_last->end ) <= 0 )
		{
			if( OK != meta_alloc_form_mempage( bd->c->page, ENTITY_BODY_BUFFER_SIZE, &new ) )
			{
				err("http body content alloc append meta failed\n");
				return ERROR;
			}
			bd->body_last->next 	= new;
			bd->body_last 			= new;
		}
		
		rc = bd->c->recv( bd->c, bd->body_last->last, meta_len( bd->body_last->last, bd->body_last->end ) );
		if( rc < 0 )
		{	
			if( rc == ERROR )
			{
				err("http body recv failed\n");
                return ERROR;
			}
			return AGAIN;
		}
		if( bd->body_cache )
		{
			bd->body_last->last 	+= rc;
		}
		bd->content_need 		-= (uint32)rc;
		//debug("content need [%d] recvd [%d]\n", bd->content_need, rc );
		if( bd->content_need <= 0 ) 
		{
			bd->body_length 		= bd->content_length;
			bd->body_status 		|= (( bd->body_cache ) ? HTTP_BODY_STAT_DONE_CACHE : HTTP_BODY_STAT_DONE_CACHENO);
			return DONE;
		}
	}
}

static status http_body_process( http_body_t * bd )
{
	uint32 remain_len = meta_len( bd->c->meta->pos, bd->c->meta->last );;

	if( bd->body_type == HTTP_BODY_TYPE_NULL )
	{
		bd->body_status |= HTTP_BODY_STAT_DONE_CACHENO;
		return DONE;
	}
    else if ( bd->body_type == HTTP_BODY_TYPE_CONTENT || bd->body_type == HTTP_BODY_TYPE_CHUNK )
    {
        if( OK != meta_alloc_form_mempage( bd->c->page, remain_len + ENTITY_BODY_BUFFER_SIZE, &bd->body_head ) )
        {
            err("http body content alloc head failed\n");
            return ERROR;
        }
        bd->body_last = bd->body_head;
        
        if( bd->body_type == HTTP_BODY_TYPE_CONTENT )
        {
            if( bd->body_cache && (remain_len > 0) )
            {
                memcpy( bd->body_last->last, bd->c->meta->pos, remain_len );
                bd->body_last->last += remain_len;
            }
            bd->content_need = bd->content_length - remain_len;
			//debug("content length [%d] need [%d] remain len [%d]\n", bd->content_length, bd->content_need, remain_len );
			bd->body_length += remain_len;
            
            // change the connection meta pos whatever cache or discard
            bd->c->meta->pos += remain_len;
            
            if( bd->content_need <= 0 )
            {
                bd->body_type |= (( bd->body_cache ) ? HTTP_BODY_STAT_DONE_CACHE : HTTP_BODY_STAT_DONE_CACHENO);
                return DONE;
            }
            
            bd->cb     = http_body_process_content;
            return bd->cb( bd );
        }
        else
        {
            if( remain_len > 0 )
            {
                memcpy( bd->body_last->last, bd->c->meta->pos, remain_len );
                bd->body_last->last += remain_len;
            }
            // change the connection meta pos whatever cache or discard
            bd->c->meta->pos += remain_len;
            
            bd->chunk_pos             = bd->body_last->pos = bd->body_last->last;
            bd->body_length           = 0;
            bd->cb     	= http_body_process_chunk;
            return bd->cb( bd );
        }
    }
    err("http body not support this type [%d]\n", bd->body_type );
	return ERROR;
}

static status http_body_alloc( http_body_t ** body )
{
	http_body_t * new;
	queue_t * q;

	if( queue_empty( &g_queue_usable ) == 1 ) 
	{
		err("http body queue usable empty\n");
		return ERROR;
	}
	q = queue_head( &g_queue_usable );
	queue_remove( q );
	queue_insert( &g_queue_use, q );
	new = ptr_get_struct( q, http_body_t, queue );
	*body = new;
	return OK;
}

status http_body_free( http_body_t * bd )
{
	bd->c 					= NULL;
	bd->state 				= 0;
	bd->cb 					= NULL;

	bd->body_type 			= HTTP_BODY_TYPE_NULL;
	bd->body_cache 			= 0;

	bd->content_length 		= 0;
	bd->content_need 		= 0;
	
	bd->hex_length 			= 0;

	bd->chunk_pos 			= NULL;
	bd->chunk_part_cur 		= 0;
	bd->chunk_part_len 		= 0;

	bd->body_head 			= NULL;
	bd->body_last 			= NULL;
	bd->body_length			= 0;
	bd->body_status			= 0;
	
	queue_remove( &bd->queue );
	queue_insert_tail( &g_queue_usable, &bd->queue );
	return OK;
}

status http_body_create( connection_t * c, http_body_t ** body, int discard )
{
	http_body_t * new;

	if( OK != http_body_alloc( &new ) ) 
	{
		err("http body alloc cycle failed\n");
		return ERROR;
	}
	new->c 				= c;
	new->state 			= 0;
	new->body_cache 	= ((discard == 1) ? 0 : 1 );
	new->cb 			= http_body_process;
	*body = new;
	return OK;
}

status http_body_init_module( void )
{
	uint32 i;
    
	g_pool = (http_body_t*)l_safe_malloc( sizeof(http_body_t)*MAX_NET_CON );
	if( !g_pool ) 
	{
		err("http body malloc g_pool\n" );
		return ERROR;
	}
	memset( g_pool, 0, sizeof(http_body_t)*MAX_NET_CON );
	
	queue_init( &g_queue_usable );
	queue_init( &g_queue_use );
	for( i = 0; i < MAX_NET_CON; i ++ ) 
	{
		queue_insert_tail( &g_queue_usable, &g_pool[i].queue );
	}
	return OK;
}

status http_body_end_module( void )
{
	if( g_pool ) 
	{
		l_safe_free( g_pool );
		g_pool = NULL;
	}
	return OK;
}
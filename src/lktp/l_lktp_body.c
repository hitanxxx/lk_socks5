#include "lk.h"

static queue_t     usable;
static queue_t     in_use;
static lktp_body_t * pool;

// lktp_body_recv ------------------------------------------
static ssize_t lktp_body_recv( lktp_body_t * lktp_body )
{
	ssize_t rc;

	rc = lktp_body->c->recv( lktp_body->c, lktp_body->body_last->last,
		meta_len( lktp_body->body_last->last, lktp_body->body_last->end ) );
	if( rc == ERROR ) {
		err_log(  "%s --- recv", __func__ );
		return ERROR;
	} else if ( rc == AGAIN ) {
		debug_log("%s --- again", __func__ );
		return AGAIN;
	} else {
		lktp_body->body_need -= rc;
		debug_log("%s --- recv [%d]", __func__, rc );
		lktp_body->body_last->last += rc;
		return rc;
	}
}
// -------
static status lktp_body_process( lktp_body_t * lktp_body )
{
    ssize_t rc;
	meta_t * new = NULL;

	while(1) {
		if( ( lktp_body->cache && !lktp_body->body_last ) ||
		lktp_body->body_last->last == lktp_body->body_last->end ) {
			if( !lktp_body->cache ) {
				lktp_body->body_last->last = lktp_body->body_last->pos;
			} else {
				if( OK != meta_alloc( &new, 8192 ) ) {
					err_log( "%s --- alloc new meta", __func__ );
					return ERROR;
				}
				if( !lktp_body->body_last ) {
					lktp_body->body_last = new;
					lktp_body->body_head = lktp_body->body_last;
				} else {
					lktp_body->body_last->next = new;
					lktp_body->body_last = lktp_body->body_last->next;
				}
			}
		}
		rc = lktp_body_recv( lktp_body );
		if( rc == ERROR ) {
			return ERROR;
		} else if ( rc == AGAIN ) {
			return AGAIN;
		}
        if( lktp_body->body_need <= 0 ) {
    		if( lktp_body->body_need < 0 ) {
    			lktp_body->body_end = lktp_body->body_last->last + lktp_body->body_need;
    		} else {
    			lktp_body->body_end = lktp_body->body_last->last;
    		}
    		return DONE;
    	}
	}
}
// ------
static status lktp_body_start( lktp_body_t * lktp_body )
{
    uint32 length, alloc_length;

	length = meta_len( lktp_body->c->meta->pos, lktp_body->c->meta->last );
	alloc_length = ( length < 8192 ) ? 8192 : length;
	if( OK != meta_alloc( &lktp_body->body_last, alloc_length ) ) {
		err_log("%s --- body_last meta alloc", __func__ );
		return ERROR;
	}
	lktp_body->body_head = lktp_body->body_last;

	lktp_body->body_need = (ssize_t)lktp_body->body_length;
	if( length ) {
		if( lktp_body->cache ) {
			memcpy( lktp_body->body_last->last, lktp_body->c->meta->pos, length );
			lktp_body->body_last->last += length;
		}
		lktp_body->body_need -= (ssize_t)length;
	}
	if( lktp_body->body_need <= 0 ) {
		if( lktp_body->body_need < 0 ) {
			lktp_body->body_end = lktp_body->body_last->last + lktp_body->body_need;
		} else {
			lktp_body->body_end = lktp_body->body_last->last;
		}
		return DONE;
	}
	lktp_body->handler = lktp_body_process;
	return lktp_body->handler ( lktp_body );
}
// -------
static status lktp_body_alloc( lktp_body_t ** lktp_body )
{
    queue_t * q;
    lktp_body_t * new;

    if( queue_empty( &usable ) ) {
        err_log("%s --- usable empty", __func__ );
        return ERROR;
    }
    q = queue_head( &usable );
    queue_remove( q );
    queue_insert_tail( &in_use, q );
    new = l_get_struct( q, lktp_body_t, queue );
    *lktp_body = new;
    return OK;
}
// --------
status lktp_body_create( connection_t *c, lktp_body_t ** lktp_body )
{
    lktp_body_t * new;

    if( OK != lktp_body_alloc( &new ) ) {
        err_log("%s --- lktp head alloc", __func__ );
        return ERROR;
    }
    new->c = c;
    new->handler = lktp_body_process;
    *lktp_body = new;
    return OK;
}
// --------
status lktp_body_free( lktp_body_t * lktp_body )
{
	meta_t *t, *n;
    queue_remove( &lktp_body->queue );
    queue_insert_tail( &usable, &lktp_body->queue );

    lktp_body->c = NULL;
    lktp_body->handler = NULL;

    lktp_body->cache = 0;
	lktp_body->body_length = 0;
	lktp_body->body_need = 0;

	lktp_body->body_end = 0;
	if( lktp_body->body_head ) {
		t = lktp_body->body_head;
		while(t) {
			n = t->next;
			meta_free( t );
			t = n;
		}
	}
	lktp_body->body_head = NULL;
	lktp_body->body_last = NULL;
    return OK;
}
// ---------
status lktp_body_init( void )
{
    uint32 i = 0;

    queue_init( &usable );
    queue_init( &in_use );
    pool = ( lktp_body_t * ) l_safe_malloc( sizeof(lktp_body_t)*MAXCON );
    if( !pool ) {
        err_log("%s --- l_safe_malloc pool", __func__ );
        return ERROR;
    }
    memset( pool, 0, sizeof(lktp_body_t)*MAXCON );
    for( i = 0; i < MAXCON; i ++ ) {
        queue_insert_tail( &usable, &pool[i].queue );
    }
    return OK;
}
// --------
status lktp_body_end( void )
{
    if( pool ) {
        l_safe_free( pool );
    }
    pool = NULL;
    return OK;
}

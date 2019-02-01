#include "lk.h"

static queue_t     usable;
static queue_t     in_use;
static lktp_head_t * pool;

// ------
static status lktp_head_recv( lktp_head_t * lktp_head, meta_t * meta )
{
    ssize_t rc;

    if( meta->last - meta->pos ) {
        return OK;
    }
    rc = lktp_head->c->recv( lktp_head->c, meta->last, meta_len( meta->last, meta->end ) );
    if( rc == ERROR ) {
        err_log("%s --- recv error", __func__ );
        return ERROR;
    } else if ( rc == AGAIN ) {
        return AGAIN;
    } else {
        meta->last += rc;
        return OK;
    }
}
// -------
static status lktp_head_parse_head( lktp_head_t * lktp_head, meta_t * meta )
{
    char * p;
    enum {
        start = 0,
        lktp_head_version,
        lktp_head_api_start,
        lktp_head_api,
        lktp_head_body_length_start,
        lktp_head_body_length
    } state;

    state = lktp_head->state;
    for( p = meta->pos; p < meta->last; p++ ) {
        if( state == start ) {
            if( '0' <= *p && *p <= '9' ) {
                lktp_head->version.data = p;
                state = lktp_head_version;
                continue;
            } else {
                err_log("%s --- lktp_head_version illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
        if( state == lktp_head_version ) {
            if( ('0' <= *p && *p <= '9') ||
                *p == '|' ) {
                if( *p == '|' ) {
                    lktp_head->version.len = meta_len( lktp_head->version.data, p );
                    state = lktp_head_api_start;
                }
                continue;
            } else {
                err_log("%s --- lktp_head_version illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
        if( state == lktp_head_api_start ) {
            if(
            ( '0' <= *p && *p <= '9' ) ||
            ( 'a' <= *p && *p <= 'z' ) ||
            ( 'A' <= *p && *p <= 'Z' ) ||
            *p == '_' ||
            *p == '-'
            ) {
                lktp_head->api.data = p;
                state = lktp_head_api;
                continue;
            } else {
                err_log("%s --- lktp_head_api_length_start illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
        if( state == lktp_head_api ) {
            if(
            ( '0' <= *p && *p <= '9' ) ||
            ( 'a' <= *p && *p <= 'z' ) ||
            ( 'A' <= *p && *p <= 'Z' ) ||
            *p == '_' ||
            *p == '-' ||
            *p == '|'
            ) {
                if( *p == '|' ) {
                    lktp_head->api.len = meta_len( lktp_head->api.data, p );
                    state = lktp_head_body_length_start;
                }
                continue;
            } else {
                err_log("%s --- lktp_head_api_length_start illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
        if( state == lktp_head_body_length_start ) {
            // body length must have one char at least
            if( ( '0' <= *p && *p <= '9' ) ) {
                lktp_head->body_length.data = p;
                state = lktp_head_body_length;
                continue;
            } else {
                err_log("%s --- lktp_head_body_length_start illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
        if( state == lktp_head_body_length ) {
            if( ( '0' <= *p && *p <= '9' ) ||
            *p == '|' ) {
                if( *p == '|' ) {
                    lktp_head->body_length.len = meta_len( lktp_head->body_length.data, p );
                    goto LKTP_HEAD_DONE;
                }
                continue;
            } else {
                err_log("%s --- lktp_head_body_length_start illegal, [%d]", __func__, *p );
                return ERROR;
            }
        }
    }
    meta->pos = p;
    lktp_head->state = state;
    return AGAIN;
LKTP_HEAD_DONE:
    meta->pos = p + 1;
    lktp_head->state = start;
    return DONE;
}
// -------
static status lktp_head_process_head( lktp_head_t * lktp_head )
{
    status rc = AGAIN;
    int32 body_length_n;

    while( 1 ) {
        if( rc == AGAIN ) {
            rc = lktp_head_recv( lktp_head, lktp_head->c->meta );
            if( rc == ERROR || rc == AGAIN ) {
                if( rc == ERROR ) {
                    err_log("%s --- lktp head recv error", __func__ );
                }
                return rc;
            }
            rc = lktp_head_parse_head( lktp_head, lktp_head->c->meta );
            if( rc == DONE ) {
                if( OK != l_atof( lktp_head->version.data, lktp_head->version.len, &lktp_head->version_n ) ) {
                    err_log("%s --- atof lktp head version, [%.*s]", __func__,
                    lktp_head->version.len, lktp_head->version.data );
                    return ERROR;
                }
                if( OK != l_atoi( lktp_head->body_length.data, lktp_head->body_length.len, &body_length_n ) ) {
                    err_log("%s --- atoi lktp head body_length, [%.*s]", __func__,
                    lktp_head->body_length.len, lktp_head->body_length.data );
                    return ERROR;
                }
                lktp_head->body_length_n = (uint32)body_length_n;
                return DONE;
            } else if ( rc == ERROR ) {
                err_log("%s --- parse head", __func__ );
                return rc;
            }
            if( lktp_head->c->meta->last <= lktp_head->c->meta->pos ) {
                err_log("%s --- head too long", __func__ );
                return ERROR;
            }
        }
    }
    return OK;
}
// -------
static status lktp_head_alloc( lktp_head_t ** lktp_head )
{
    queue_t * q;
    lktp_head_t * new;

    if( queue_empty( &usable ) ) {
        err_log("%s --- usable empty", __func__ );
        return ERROR;
    }
    q = queue_head( &usable );
    queue_remove( q );
    queue_insert_tail( &in_use, q );
    new = l_get_struct( q, lktp_head_t, queue );
    *lktp_head = new;
    return OK;
}
// --------
status lktp_head_create( connection_t *c, lktp_head_t ** lktp_head )
{
    lktp_head_t * new;

    if( OK != lktp_head_alloc( &new ) ) {
        err_log("%s --- lktp head alloc", __func__ );
        return ERROR;
    }
    new->c = c;
    new->handler = lktp_head_process_head;
    new->state = 0;
    *lktp_head = new;
    return OK;
}
// --------
status lktp_head_free( lktp_head_t * lktp_head )
{
    queue_remove( &lktp_head->queue );
    queue_insert_tail( &usable, &lktp_head->queue );

    lktp_head->c = NULL;
    lktp_head->state = 0;
    lktp_head->handler = NULL;

    lktp_head->version_n = 0;
    lktp_head->version.len = 0;
    lktp_head->version.data = NULL;

    lktp_head->api.len = 0;
    lktp_head->api.data = NULL;

    lktp_head->body_length_n = 0;
    lktp_head->body_length.len = 0;
    lktp_head->body_length.data = NULL;

    return OK;
}
// ---------
status lktp_head_init( void )
{
    uint32 i = 0;

    queue_init( &usable );
    queue_init( &in_use );
    pool = ( lktp_head_t * ) l_safe_malloc( sizeof(lktp_head_t)*MAXCON );
    if( !pool ) {
        err_log("%s --- l_safe_malloc pool", __func__ );
        return ERROR;
    }
    memset( pool, 0, sizeof(lktp_head_t)*MAXCON );
    for( i = 0; i < MAXCON; i ++ ) {
        queue_insert_tail( &usable, &pool[i].queue );
    }
    return OK;
}
// --------
status lktp_head_end( void )
{
    if( pool ) {
        l_safe_free( pool );
    }
    pool = NULL;
    return OK;
}

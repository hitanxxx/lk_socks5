#ifndef _L_QUEUE_H_INCLUDED_
#define _L_QUEUE_H_INCLUDED_

typedef struct queue_t queue_t;
typedef struct queue_t
{
    queue_t  *prev;
    queue_t  *next;
} queue_t;

int32 queue_get_num( queue_t * queue );
int32 queue_empty( queue_t * h );
void queue_init( queue_t * q );
void queue_insert( queue_t * h, queue_t * q );
void queue_insert_tail( queue_t * h, queue_t * q );
void queue_remove( queue_t * q );
queue_t * queue_head( queue_t * h );
queue_t * queue_next( queue_t * q );
queue_t * queue_prev( queue_t * q );
queue_t * queue_tail( queue_t * h );

#endif

#include "lk.h"

// queue_init --------------------
void queue_init( queue_t * q )
{
	q->prev = q;
	q->next = q;
}
// queue_insert ------------------
void queue_insert( queue_t * h, queue_t * q )
{
	q->next = h->next;
	q->prev = h;

	h->next->prev = q;
	h->next = q;
}
// queue_insert_tail --------------
void queue_insert_tail( queue_t * h, queue_t * q )
{
	queue_t * last;

	last = h->prev;

	q->next = last->next;
	q->prev = last;

	last->next->prev = q;
	last->next = q;
}
// queue_remove ------------------
void queue_remove( queue_t * q )
{
	q->prev->next = q->next;
	q->next->prev = q->prev;

	q->prev = NULL;
	q->next = NULL;
}
// queue_empty -------------------
status queue_empty( queue_t * h )
{
	if( h == h->prev ) {
		return 1;
	} else {
		return 0;
	}
}
// queue_head -------------------
queue_t * queue_head( queue_t * h )
{
	return h->next;
}
// queue_next -------------------
queue_t * queue_next( queue_t * q )
{
	return q->next;
}
// queue_prev -------------------
queue_t * queue_prev( queue_t * q )
{
	return q->prev;
}
// queue_tail --------------------
queue_t * queue_tail( queue_t * h )
{
	return h;
}

#include "common.h"

void queue_init(queue_t *q) {
    q->prev = q;
    q->next = q;
}

void queue_insert(queue_t *h, queue_t *q) {
    q->next = h->next;
    q->prev = h;

    h->next->prev = q;
    h->next = q;
}

void queue_insert_tail(queue_t *h, queue_t *q) {
    queue_t *last;

    last = h->prev;

    q->next = last->next;
    q->prev = last;

    last->next->prev = q;
    last->next = q;
}

void queue_remove(queue_t *q) {
    if (q->prev) 
        q->prev->next = q->next;

    if (q->next)
        q->next->prev = q->prev;

    q->prev = NULL;
    q->next = NULL;
}

status queue_empty(queue_t *h) {
    return ((h == h->prev) ? 1 : 0);
}

queue_t *queue_head(queue_t *h) {
    return h->next;
}

queue_t *queue_next(queue_t *q) {
    return q->next;
}

queue_t *queue_prev(queue_t *q) {
    return q->prev;
}

queue_t *queue_tail(queue_t * h) {
    return h;
}


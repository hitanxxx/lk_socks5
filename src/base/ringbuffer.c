#include "common.h"

/// @brief create a ringbuffer, all space is size. ringbuffer means memory buffer
/// @param rb
/// @param size
/// @return
int ringbuffer_alloc(ringbuffer_t **rb, int size)
{
    ringbuffer_t * nrb = mem_pool_alloc(sizeof(ringbuffer_t) + size);
    if (!nrb) {
        err("rb alloc err.\n");
        return -1;
    }
    nrb->space = size;
    nrb->datan = 0;
    nrb->pos = nrb->last = nrb->start = nrb->data;
    nrb->end = nrb->start + size;
    pthread_mutex_init(&nrb->lock, NULL);
    *rb = nrb;
    return 0;
}

/// @brief free a ringbuffer
/// @param rb
void ringbuffer_free(ringbuffer_t *rb)
{
    if (rb) mem_pool_free(rb);
}

int rb_full(ringbuffer_t * rb)
{
    return (rb->space == rb->datan ? 1 : 0);
}

int rb_empty(ringbuffer_t * rb)
{
    return (rb->datan == 0 ? 1 : 0);
}

int ringbuffer_push(ringbuffer_t *rb,unsigned char *data, int datan)
{
    pthread_mutex_lock(&rb->lock);
    if(rb_full(rb)) {
        pthread_mutex_unlock(&rb->lock);
        err("rb full\n");
        return -1;
    }

    if(datan > rb->space - rb->datan) {
        pthread_mutex_unlock(&rb->lock);
        err("rb free space not enough\n");
        return -1;
    }

    if(rb->pos <= rb->last) {
        /// start-----pos-----last-----end
        int tailn = rb->end - rb->last;
        if(tailn >= datan) {
            memcpy(rb->last, data, datan);
            rb->last += datan;
            rb->datan += datan;
        } else {
            if(tailn) {
                memcpy(rb->last, data, tailn);
                rb->last += tailn;
                rb->datan += tailn;
                rb->last = rb->start;
            }
            int headn = datan - tailn;
            memcpy(rb->start, data + tailn, headn);
            rb->last += headn;
            rb->datan += headn;    
        }
    } else {
        /// start-----last-----pos-----end
        int freen = rb->pos - rb->last;
        memcpy(rb->last, data, datan);
        rb->last += freen;
        rb->datan += freen;
    }
    return 0;
}

int ringbuffer_pull(ringbuffer_t *rb, int datan, unsigned char *data, int *outn)
{
    pthread_mutex_lock(&rb->lock);
    if(rb_empty(rb)) {
        pthread_mutex_unlock(&rb->lock);
        err("rb empty\n");
        return -1;
    }    

    if(rb->pos < rb->last) {
        /// start-----pos-----last-----end
        int pulln = (datan >= (rb->last - rb->pos) ? (rb->last - rb->pos) : datan);
        memcpy(data, rb->pos, pulln);
        rb->pos += pulln;
        rb->datan -= pulln;
        *outn = pulln;
    } else {
        /// start-----last-----pos-----end
        int tailn = rb->end - rb->pos;
        if(tailn >= datan) {
            memcpy(data, rb->pos, datan);
            rb->pos += datan;
            rb->datan -= datan;

            *outn = datan;
        } else {
            // copy all tail
            if(tailn) {
                memcpy(data, rb->pos, tailn);
                rb->pos += tailn;
                rb->datan -= tailn;
                rb->pos = rb->start;
            }
            int headn = rb->last - rb->start;
            int pulln = (datan - tailn >= headn ? headn : (datan - tailn));
            memcpy(data + tailn, rb->start, pulln);
            rb->pos += pulln;
            rb->datan -= pulln;

            *outn = tailn + pulln;
        }
    }
    return 0;
}

int ringbuffer_getlen(ringbuffer_t * rb)
{
    return (rb->datan);
}

int ringbuffer_getfree(ringbuffer_t * rb)
{
    return (rb->space - rb->datan);
}


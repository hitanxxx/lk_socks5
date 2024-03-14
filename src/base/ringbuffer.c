#include "common.h"

/// @brief create a ringbuffer, all space is size. ringbuffer means memory buffer
/// @param rb
/// @param size
/// @return
int ringbuffer_alloc(ringbuffer_t **rb, int size)
{
	ringbuffer_t *rb_n = calloc(1, sizeof(ringbuffer_t) + size);
	if (!rb_n) {
		err("alloc ringbuffer failed. [%d]\n", errno);
		return ERROR;
	}
    rb_n->space = size;
    rb_n->datan = 0;
    rb_n->pos = rb_n->last = rb_n->start = rb_n->data;
    rb_n->end = rb_n->start + size;

	*rb = rb_n;
	return OK;
}

/// @brief free a ringbuffer
/// @param rb
void ringbuffer_free(ringbuffer_t *rb)
{
	if (rb) free(rb);
}

static int rb_full( ringbuffer_t * rb )
{
	return ( rb->space == rb->datan ? 1 : 0);
}

int rb_empty( ringbuffer_t * rb )
{
	return rb->datan == 0 ? 1 : 0;
}
int ringbuffer_push(ringbuffer_t *rb,unsigned char *data, int datan)
{
	if( rb_full(rb)) {
		err("rb full\n");
		return -1;
	}

	if( datan > rb->space - rb->datan ) {
		err("rb free space not enough\n");
		return -1;
	}

	if( rb->pos <= rb->last ) {
		/// start-----pos-----last-----end
		int tailn = rb->end - rb->last;
		if( tailn >= datan ) {
			memcpy( rb->last, data, datan );
			rb->last += datan;
			rb->datan += datan;
		} else {
			memcpy( rb->last, data, tailn );
			rb->last += tailn;
			rb->datan += tailn;

			int headn = datan - tailn;
			memcpy( rb->start, data + tailn, headn );
		    rb->last = rb->start + headn;
			rb->datan += headn;	
		}
	} else {
		/// start-----last-----pos-----end
		int freen = rb->pos - rb->last;
		memcpy( rb->last, data, datan );
	    rb->last += freen;
		rb->datan += freen;
	}
	return 0;
}

int ringbuffer_pull(ringbuffer_t *rb, int datan, unsigned char *data, int *outn)
{
	if( rb_empty(rb)) {
		err("rb empty\n");
		return -1;
	}	

	if( rb->pos < rb->last ) {
		/// start-----pos-----last-----end
		int pulln = ( datan >= (rb->last - rb->pos) ? (rb->last-rb->pos) : datan );
		memcpy( data, rb->pos, pulln );
		rb->pos += pulln;
		rb->datan -= pulln;

		*outn = pulln;
	} else {
		/// start-----last-----pos-----end
		int tailn = rb->end - rb->pos;
		if( tailn >= datan ) {
			memcpy( data, rb->pos, datan );
			rb->pos += datan;
			rb->datan -= datan;

			*outn = datan;
		} else {
			// copy all tail
			memcpy( data, rb->pos, tailn );
		        rb->pos += tailn;
			rb->datan -= tailn;
			
			int headn = rb->last - rb->start;
			int pulln = ( datan - tailn >= headn ? headn : (datan - tailn) );
			memcpy( data + tailn, rb->start, pulln );
		    rb->pos += pulln;
			rb->datan -= pulln;

			*outn = tailn + pulln;
		}
	}
	return 0;
}


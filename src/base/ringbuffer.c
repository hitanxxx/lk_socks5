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
	rb_n->size = size;
	rb_n->data_len = 0;
	rb_n->pos = rb_n->last = rb_n->start = rb_n->data;
	rb_n->end = rb_n->start + size;

	*rb = rb_n;
	return OK;
}

/// @brief free a ringbuffer
/// @param rb
void ringbuffer_free(ringbuffer_t *rb)
{
	if (rb)
		free(rb);
}

/// @brief push data into ringbuffer
/// @param rb
/// @param data storge into ringbuffer data
/// @param len size of storge into ringbuffer data
/// @return
int ringbuffer_push(ringbuffer_t *rb, char *data, int len)
{
	int space_tail = 0;

	if (len > (rb->size - rb->data_len)) {
		err("rb full\n");
		return ERROR;
	}
	space_tail = rb->end - rb->last;
	if (space_tail >= len) {
		memcpy(rb->last, data, len);
		rb->last += len;
	} else {
		memcpy(rb->last, data, space_tail);
		memcpy(rb->start, data + space_tail, len - space_tail);
		rb->last = rb->start + (len - space_tail);
	}
	rb->data_len += len;
	return OK;
}

/// @brief pull data form ringbuffer
/// @param rb  point to ringbuffer
/// @param len  want length
/// @param out_data space for storge outer data
/// @param out_len size of outer data, mabey less than want length if ringbuffer data not enough
/// @return
int ringbuffer_pull(ringbuffer_t *rb, int len, char *out_data, int *out_len)
{
	int space_tail = 0;
	int want_len = 0;

	*out_len = 0;

	if (rb->data_len <= 0) {
		err("rb empty\n");
		return ERROR;
	}

	if (len > rb->data_len) {
		want_len = rb->data_len;
	} else {
		want_len = len;
	}

	space_tail = rb->end - rb->pos;
	if (space_tail > want_len) {
		memcpy(out_data, rb->pos, want_len);
		rb->pos += want_len;
	} else {
		memcpy(out_data, rb->pos, space_tail);
		memcpy(out_data, rb->start, want_len - space_tail);
		rb->pos = rb->start + (want_len - space_tail);
	}

	rb->data_len -= want_len;
	*out_len = want_len;
	return OK;
}

/// @brief check ringbuffer empty or not
/// @param rb
/// @return
int ringbuffer_empty(ringbuffer_t *rb)
{
	return (rb->data_len == 0) ? 1 : 0;
}

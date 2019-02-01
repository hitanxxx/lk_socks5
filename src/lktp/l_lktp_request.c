#include "lk.h"

// lktp_request_create ----------
status lktp_request_create( lktp_request_t * lktp_request )
{
	string_t version = string("1|");
	uint32 body_len = 0, alloc_length = 0;
	char body_len_arr[64] = {0};
	meta_t * new, *cl;
	char * p;

	cl = lktp_request->body_raw;
	while( cl ) {
		body_len += meta_len( cl->pos, cl->last );
	}
	snprintf( body_len_arr, 64, "%d", body_len );

	alloc_length += version.len;
	alloc_length += lktp_request->api->len;
	alloc_length += l_strlen("|");
	alloc_length += l_strlen(body_len_arr);
	alloc_length += l_strlen("|");

	if( OK != meta_page_alloc( lktp_request->page, alloc_length, &new ) ) {
		err_log("%s --- meta_page_alloc failed", __func__ );
		return ERROR;
	}
	p = new->data;
	l_memcpy( p, version.data, version.len );
	p += version.len;
	l_memcpy( p, lktp_request->api->data, lktp_request->api->len );
	p += lktp_request->api->len;
	l_memcpy( p, "|", l_strlen("|") );
	p += l_strlen("|");
	l_memcpy( p, body_len_arr, l_strlen(body_len_arr) );
	p += l_strlen(body_len_arr);
	l_memcpy( p, "|", l_strlen("|") );
	p += l_strlen("|");
	new->last = p;

	if( lktp_request->body_raw ) {
		new->next = lktp_request->body_raw;
	}
	lktp_request->lktp_request_chain = new;
#if(0)
	meta_t * n;
	n = lktp_request->lktp_request_chain;
	while( n ) {
		debug_log("%s --- [%.*s]", __func__, meta_len( n->pos, n->last ), n->pos );
		n = n->next;
	}
#endif
	return OK;
}

#ifndef _L_LKTP_REQUEST_H_INCLUDED_
#define _L_LKTP_REQUEST_H_INCLUDED_

typedef struct lktp_request_t {
	l_mem_page_t * 	page;
	string_t *		api;
	meta_t 	* 		body_raw;

	meta_t *		lktp_request_chain;
} lktp_request_t;

status lktp_request_create( lktp_request_t * lktp_request );

#endif

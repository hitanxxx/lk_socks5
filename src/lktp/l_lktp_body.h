#ifndef _L_LKTP_BODY_H_INCLUDED_
#define _L_LKTP_BODY_H_INCLUDED_


typedef struct lktp_body_t lktp_body_t;
typedef status ( *lktp_body_handler )( lktp_body_t * head );
struct lktp_body_t {
    queue_t             queue;
    connection_t *      c;
    lktp_body_handler   handler;

    uint32      cache;

    uint32      body_length;
    ssize_t     body_need;

    meta_t *    body_head;
    meta_t *    body_last;
    char*       body_end;
};

status lktp_body_create( connection_t * c, lktp_body_t ** lktp_body );
status lktp_body_free( lktp_body_t * lktp_body );

status lktp_body_init( void );
status lktp_body_end( void );

#endif

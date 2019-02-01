#ifndef _L_LKTP_HEAD_H_INCLUDED_
#define _L_LKTP_HEAD_H_INCLUDED_

/*
    a very simple transfer poroctol's message format
    version:v0.1

    number   string   number    ~
    version | api | bodylength | bodyraw
*/

typedef struct lktp_head_t lktp_head_t;
typedef status ( *lktp_head_handler )( lktp_head_t * head );
struct lktp_head_t {
    queue_t             queue;
    connection_t *      c;
    uint32              state;
    lktp_head_handler   handler;

    string_t    version;
    float       version_n;

    string_t    api;

    string_t    body_length;
    uint32      body_length_n;
};

status lktp_head_create( connection_t * c, lktp_head_t ** lktp_head );
status lktp_head_free( lktp_head_t * lktp_head );

status lktp_head_init( void );
status lktp_head_end( void );

#endif

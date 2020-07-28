#ifndef _L_SERV_H_INCLUDED_
#define _L_SERV_H_INCLUDED_

typedef status ( * serv_api_handler  ) ( event_t * ev );
typedef struct serv_api_t {
	string_t 			name;
	serv_api_handler	handler;
} serv_api_t;

status serv_init( void );
status serv_end( void );
status serv_api_register( const char * api_key, serv_api_handler handler );
status serv_api_find( string_t * key, serv_api_handler * handler );

#endif

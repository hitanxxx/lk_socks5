#ifndef _L_LOG_H_INCLUDED_
#define _L_LOG_H_INCLUDED_

#define LOG_TIME_LENGTH		strlen("[0000/00/00]-[00:00:00]-")
#define LOG_LEVEL_LENGTH	strlen("[ xxxxx ]-")
#define	LOG_TEXT_LENGTH		1024
#define	LOG_LENGTH	LOG_TIME_LENGTH + LOG_LEVEL_LENGTH + LOG_TEXT_LENGTH

#define LOG_ID_MAIN		0
#define LOG_ID_ACCESS	1

#define LOG_LEVEL_ERROR		0
#define LOG_LEVEL_DEBUG		1
#define LOG_LEVEL_INFO		2

#define err_log( ... ) \
  		(l_log( LOG_ID_MAIN, LOG_LEVEL_ERROR, __VA_ARGS__ ))
#define debug_log( ... ) \
		(l_log( LOG_ID_MAIN, LOG_LEVEL_DEBUG, __VA_ARGS__ ))
#define access_log( ... ) \
		(l_log( LOG_ID_ACCESS, LOG_LEVEL_INFO, __VA_ARGS__ ))

typedef struct log_content_t {
	uint32		level;
	uint32		id;

	char * 		p;
	char * 		last;
	va_list		args_list;
	const char* args;
} log_content_t;

status l_log( uint32 id, uint32 level, const char * str, ... );
status log_init( void );
status log_end( void );

#endif

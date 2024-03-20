#ifndef _LOG_H_INCLUDED_
#define _LOG_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    
#define	LOG_TEXT_LENGTH     512

#define LOG_ID_MAIN			0
#define LOG_ID_ACCESS		1

enum e_log_level
{   
    LOG_LEVEL_ERROR = 0x0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
};

#define err( ... )  		{(log_print( LOG_ID_MAIN, LOG_LEVEL_ERROR, __func__, __LINE__, ##__VA_ARGS__ ));}
#define dbg( ... )  		{(log_print( LOG_ID_MAIN, LOG_LEVEL_DEBUG, __func__, __LINE__, ##__VA_ARGS__ ));}
#define access_log( ... ) 	{(log_print( LOG_ID_ACCESS, LOG_LEVEL_INFO, __func__, __LINE__, ##__VA_ARGS__ ));}


typedef struct 
{
	uint32		level;
	uint32		id;

	char 		func[64];
	int			line;

	va_list		args_list;
	const char* args;

	char * 		pos;
	char * 		last;
} log_content_t;

status log_print( uint32 id, uint32 level,  const char * func, int line, const char * str, ... );
status log_init( void );
status log_end( void );

#ifdef __cplusplus
}
#endif
    
#endif

#include "lk.h"

static int32 log_fd_main = 0;
static int32 log_fd_access = 0;

static string_t levels[] = {
	string("[ ERROR ]"),
	string("[ DEBUG ]"),
	string("[ INFO  ]")
};

// log_format_time ----------
static status log_format_time( log_content_t * log_content )
{
	snprintf( log_content->p, LOG_TIME_LENGTH + 1, "%.*s",
		cache_time_log.len,
		cache_time_log.data
	);
	log_content->p += LOG_TIME_LENGTH;
	return OK;
}
// log_format_level -----------
static status log_format_level( log_content_t * log_content )
{
	snprintf( log_content->p, LOG_LEVEL_LENGTH + 1, "%.*s-",
		levels[log_content->level].len,
		levels[log_content->level].data
	);
	log_content->p += LOG_LEVEL_LENGTH;
	return OK;
}
// log_format_text ------------
static status log_format_text( log_content_t * log_content )
{
	int32 length = 0;

	length += snprintf( log_content->p + length, LOG_TEXT_LENGTH - (size_t)length, "[%s]-", log_content->func );
	length += vsnprintf( log_content->p + length, LOG_TEXT_LENGTH - (size_t)length, log_content->args, log_content->args_list );

	length = ( length <= LOG_TEXT_LENGTH ) ? length : LOG_TEXT_LENGTH;
	log_content->p += length;
	
	return OK;
}
// log_write_stdout --------
static status log_write_stdout( char * str, int32 length )
{
	ssize_t rc;

	rc = write( STDOUT_FILENO, str, (size_t)length );
	if( rc == ERROR ) {
		return ERROR;
	}
	return OK;
}
// log_write_file_main -------
static status log_write_file_main( char * str, int32 length )
{
	ssize_t rc;

	if( log_fd_main > 0 ) {
		rc = write( log_fd_main, str, (size_t)length );
		if( rc == ERROR ) {
			return ERROR;
		}
	}
	return OK;
}
// log_write_file_access -------
static status log_write_file_access( char * str, int32 length )
{
	ssize_t rc;

	if( log_fd_access > 0 ) {
		rc = write( log_fd_access, str, (size_t)length );
		if( rc == ERROR ) {
			return ERROR;
		}
	}
	return OK;
}
// l_log   ---------------
status l_log( uint32 id, uint32 level, const char * func, const char * str, ... )
{
	log_content_t log_content;
	char 	stream[LOG_LENGTH];

	if( id == LOG_ID_ACCESS ) {
		if( conf.http_access_log != 1 ) return OK;
	}
	switch(level) {
		case LOG_LEVEL_DEBUG:
			if( conf.log_debug != 1 ) return OK;
			break;
		case LOG_LEVEL_ERROR:
			if( conf.log_error != 1 ) return OK;
			break;
		default:;
	}
	va_start( log_content.args_list, str );
	log_content.id = id;
	log_content.level = level;
	log_content.p = stream;
	log_content.last = stream + LOG_LENGTH;
	log_content.args = str;
	strncpy( log_content.func, func,  sizeof(log_content.func) );
	
	log_format_time( &log_content );
	log_format_level( &log_content );
	log_format_text( &log_content );
	va_end( log_content.args_list );

	log_write_stdout( stream, (int32)( log_content.p - stream) );
	// decide which file to write
	switch(id){
		case LOG_ID_MAIN:
			log_write_file_main( stream, (int32)( log_content.p - stream) );
			break;
		case LOG_ID_ACCESS:
			log_write_file_access( stream, (int32)( log_content.p - stream) );
			break;
		default:;
	}
	return OK;
}
// log_init ------------------------------
status log_init( void )
{
	log_fd_main = open( L_PATH_LOG_MAIN, O_CREAT|O_RDWR|O_APPEND, 0644 );
	if( log_fd_main == ERROR ) {
		err( "open logfile-(main) [%s], [%d]",
		L_PATH_LOG_MAIN, errno );
		return ERROR;
	}
	log_fd_access = open( L_PATH_LOG_ACCESS, O_CREAT|O_RDWR|O_APPEND, 0644 );
	if( log_fd_access == ERROR ) {
		err( "open logfile-(access) [%s], [%d]",
		L_PATH_LOG_ACCESS, errno );
		return ERROR;
	}
	return OK;
}
// log_end ----------------------
status log_end( void )
{
	close( log_fd_main );
	close( log_fd_access );
	return OK;
}

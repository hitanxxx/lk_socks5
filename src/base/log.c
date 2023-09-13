#include "common.h"


typedef struct 
{
	int 	log_fd_main;
	int 	log_fd_access;
} log_mgr_t;

log_mgr_t * log_ctx = NULL;

static string_t levels[] = {
	string("[ERR]"),
	string("[DBG]"),
	string("[PRI]")
};

static status log_format_time( log_content_t * ctx )
{
	int len = snprintf( ctx->pos, meta_len( ctx->pos, ctx->last )-1, " %s", systime_log() );
	ctx->pos += len;
	return OK;
}

static status log_format_level( log_content_t * ctx )
{
	int len = snprintf( ctx->pos, meta_len( ctx->pos, ctx->last )-1, "%.*s",levels[ctx->level].len,levels[ctx->level].data);
	ctx->pos += len;
	return OK;
}

static status log_format_text( log_content_t * ctx )
{
	int32 len = 0;

	len = snprintf( ctx->pos, meta_len( ctx->pos, ctx->last )-1, " %s", ctx->func );
	ctx->pos += len;
	len = snprintf( ctx->pos, meta_len( ctx->pos, ctx->last )-1, ":%d ", ctx->line );
	ctx->pos += len;
	len = vsnprintf( ctx->pos, meta_len( ctx->pos, ctx->last )-1, ctx->args, ctx->args_list );
	ctx->pos += len;
	return OK;
}

static status log_write_stdout( char * str, int32 length )
{
	ssize_t rc = write( STDOUT_FILENO, str, (size_t)length );
	if( rc == ERROR ) 
		return ERROR;
	return OK;
}

static status log_write_file_main( char * str, int32 length )
{
	ssize_t rc;

	if( log_ctx && (log_ctx->log_fd_main > 0) ) {
		rc = write( log_ctx->log_fd_main, str, (size_t)length );
		if( rc == ERROR ) 
			return ERROR;
	}
	return OK;
}

static status log_write_file_access( char * str, int32 length )
{
	ssize_t rc;

	if( log_ctx && (log_ctx->log_fd_access > 0) ) {
		rc = write( log_ctx->log_fd_access, str, (size_t)length );
		if( rc == ERROR )
			return ERROR;
	}
	return OK;
}

status log_print( uint32 id, uint32 level, const char * func, int line, const char * str, ... )
{
    log_content_t 	ctx;
    char    buffer[LOG_TEXT_LENGTH] = {0};

    if( level <= config_get()->sys_log_level ) {
        memset( &ctx, 0, sizeof(log_content_t) );
        va_start( ctx.args_list, str );
        ctx.id 		= id;
        ctx.level 	= level;
        ctx.pos 	= buffer;
        ctx.last 	= buffer + LOG_TEXT_LENGTH;
        snprintf( ctx.func, sizeof(ctx.func)-1, "%s", func );
        ctx.line 	= line;
        ctx.args 	= str;

        log_format_level( &ctx );
        log_format_time( &ctx );
        log_format_text( &ctx );
        va_end( ctx.args_list );

        log_write_stdout( buffer, (int32)( ctx.pos - buffer) );
        
        if( id == LOG_ID_MAIN )
            log_write_file_main( buffer, (int32)( ctx.pos - buffer) );
        else if ( id == LOG_ID_ACCESS )
            log_write_file_access( buffer, (int32)( ctx.pos - buffer) );
        
    }

	return OK;
}

status log_init( void )
{
    do {
        if( log_ctx ) {
            err("log ctx not null\n");
            break;
        }
        log_ctx = l_safe_malloc( sizeof( log_mgr_t ) );
        if( !log_ctx ) {
            err("malloc log ctx failed. [%d]\n", errno );
            break;
        }

        log_ctx->log_fd_main = open( L_PATH_LOG_MAIN, O_CREAT|O_RDWR|O_APPEND, 0644 );
        if( log_ctx->log_fd_main <= 0 ) {
            err( "open logfile [%s] failed, [%d]\n", L_PATH_LOG_MAIN, errno );
            break;
        }
        log_ctx->log_fd_access = open( L_PATH_LOG_ACCESS, O_CREAT|O_RDWR|O_APPEND, 0644 );
        if( log_ctx->log_fd_access <= 0 ) {
            err( "open logfile [%s] failed, [%d]\n", L_PATH_LOG_ACCESS, errno );
            break;
        }
        return OK;
    } while(0);

    if( log_ctx ) {
        if( log_ctx->log_fd_main )
            close(log_ctx->log_fd_main);
        if(log_ctx->log_fd_access)
            close(log_ctx->log_fd_access);
       l_safe_free(log_ctx);
    }

    return ERROR;

}

status log_end( void )
{
    if( log_ctx ) {
        if( log_ctx->log_fd_main > 0 )
            close( log_ctx->log_fd_main );
        if( log_ctx->log_fd_access > 0 )
            close( log_ctx->log_fd_access );
        l_safe_free(log_ctx);
    }
    return OK;
}

#include "common.h"


typedef struct 
{
    int 	log_fd_main;
    int 	log_fd_access;
} log_mgr_t;

log_mgr_t * log_ctx = NULL;

static string_t levels[] = {
    string("[error]"),
    string("[debug]"),
    string("[info]")
};

static int log_format_prefix(log_content_t *ctx)
{
    int n = snprintf(ctx->pos, meta_len(ctx->pos, ctx->last)-1, "%.*s %s <%s:%d> ",
                levels[ctx->level].len,levels[ctx->level].data,
                systime_log(),
                ctx->func, ctx->line
            );
    ctx->pos += n;
    return 0;
}

static int log_format_text(log_content_t * ctx)
{
    int n = vsnprintf(ctx->pos, meta_len(ctx->pos, ctx->last)-1, ctx->args, ctx->args_list);
    ctx->pos += n;
    return 0;
}

static int log_write_stdout(char * str, int32 strn)
{
    int rc = write(STDOUT_FILENO, str, (size_t)strn);
    if(rc == -1) 
        return -1;
    return 0;
}

static int log_write_file_main(char * str, int32 strn)
{
    ssize_t rc;
    if(log_ctx && (log_ctx->log_fd_main > 0)) {
        rc = write(log_ctx->log_fd_main, str, (size_t)strn);
        if(rc == -1) 
            return -1;
    }
    return 0;
}

static status log_write_file_access(char * str, int32 strn)
{
    ssize_t rc;
    if(log_ctx && (log_ctx->log_fd_access > 0)) {
        rc = write(log_ctx->log_fd_access, str, (size_t)strn);
        if(rc == -1)
            return -1;
    }
    return 0;
}

int log_print(int id, int level, const char * func, int line, const char * str, ...)
{
    log_content_t ctx;
    char buffer[LOG_TEXT_LENGTH] = {0};

    if(level <= config_get()->sys_log_level) {
        memset(&ctx, 0, sizeof(log_content_t));
        va_start(ctx.args_list, str);
        ctx.id 		= id;
        ctx.level 	= level;
        ctx.pos 	= buffer;
        ctx.last 	= buffer + LOG_TEXT_LENGTH;
        snprintf(ctx.func, sizeof(ctx.func)-1, "%s", func);
        ctx.line 	= line;
        ctx.args 	= str;

        log_format_prefix(&ctx);
        log_format_text(&ctx);
        va_end(ctx.args_list);
        log_write_stdout(buffer, (int)(ctx.pos - buffer));
        
        if(id == LOG_ID_MAIN)
            log_write_file_main(buffer, (int)(ctx.pos - buffer));
        else if (id == LOG_ID_ACCESS)
            log_write_file_access(buffer, (int)(ctx.pos - buffer));
    }
    return 0;
}

int log_init(void)
{
    do {
        if(log_ctx) {
            err("log ctx not null\n");
            break;
        }
        log_ctx = sys_alloc(sizeof(log_mgr_t));
        if(!log_ctx) {
            err("malloc log ctx failed. [%d]\n", errno);
            break;
        }
        ///O_APPEND make write is atomic operation
        log_ctx->log_fd_main = open(S5_PATH_LOG_FILE_MAIN, O_CREAT|O_RDWR|O_APPEND, 0644);
        if(log_ctx->log_fd_main <= 0) {
            err( "open logfile [%s] failed, [%d]\n", S5_PATH_LOG_FILE_MAIN, errno);
            break;
        }
        log_ctx->log_fd_access = open(S5_PATH_LOG_FILE_ACCESS, O_CREAT|O_RDWR|O_APPEND, 0644);
        if(log_ctx->log_fd_access <= 0) {
            err( "open logfile [%s] failed, [%d]\n", S5_PATH_LOG_FILE_ACCESS, errno);
            break;
        }
        return 0;
    } while(0);

    if(log_ctx) {
        if(log_ctx->log_fd_main)
            close(log_ctx->log_fd_main);
        if(log_ctx->log_fd_access)
            close(log_ctx->log_fd_access);
        sys_free(log_ctx);
        log_ctx = NULL;
    }
    return -1;
}

int log_end(void)
{
    if(log_ctx) {
        if(log_ctx->log_fd_main)
            close(log_ctx->log_fd_main);
        if(log_ctx->log_fd_access)
            close(log_ctx->log_fd_access);
        sys_free(log_ctx);
        log_ctx = NULL;
    }
    return OK;
}


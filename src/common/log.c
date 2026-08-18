#include "common.h"

typedef struct {
    int log_fd_main;
    int log_fd_access;
} log_mgr_t;

static log_mgr_t *log_ctx = NULL;
static char *levels[] = {"[ERR]", "[DBG]", "[INF]"};

static int log_format_prefix(log_content_t *ctx) {
    int ret = snprintf(ctx->pos, (ctx->last - ctx->pos) - 1, "%s %s (%d) %s#%d: ",
                        levels[ctx->level],
                        systime_log(),
                        (g_proc_ctx ?  (g_proc_ctx->pcur ? g_proc_ctx->pcur->pid : getpid()) : getpid()),
                        ctx->func,
                        ctx->line);
    if (ret > 0) {
        int written = (ret < (ctx->last - ctx->pos) ? ret : (ctx->last - ctx->pos - 1));
        ctx->pos += written;
    }
    return 0;
}

static int log_format_text(log_content_t *ctx) {
    int ret = vsnprintf(ctx->pos, (ctx->last - ctx->pos) - 1, ctx->args, ctx->args_list);
    if (ret > 0) {
        int writern = (ret < (ctx->last - ctx->pos) ? ret : (ctx->last - ctx->pos - 1));
        ctx->pos += writern;
    }
    return 0;
} 

static int log_write(int fd, char *str, uint32_t strn) {
    if (fd >= 0) {
        int ret = write(fd, str, strn);
        return (ret == -1 ? -1 : 0);
    }
    return 0;
}

static int log_write_stdout(char *str, uint32_t strn) {
    return log_write(STDOUT_FILENO, str, strn);
}

static int log_write_file_main(char *str, uint32_t strn) {
    return (log_ctx ? log_write(log_ctx->log_fd_main, str, strn) : 0);
}

static int log_write_file_access(char *str, uint32_t strn) {
    return (log_ctx ? log_write(log_ctx->log_fd_access, str, strn) : 0);
}

int log_print(int id, int level, const char *func, int line, const char *str, ...) {

    if (level <= config_get()->sys_log_level) {
        char buffer[LOG_TEXT_LENGTH] = {0};
        log_content_t ctx = {0};
        ctx.id = id;
        ctx.level = level;
        ctx.pos = buffer;
        ctx.last = buffer + LOG_TEXT_LENGTH;
        ctx.line = line;
        ctx.args = str;
        ctx.func = func;
        va_start(ctx.args_list, str);
        log_format_prefix(&ctx);
        log_format_text(&ctx);
        va_end(ctx.args_list);

        log_write_stdout(buffer, (int)(ctx.pos - buffer));
        if (id == LOG_ID_MAIN) log_write_file_main(buffer, (int)(ctx.pos - buffer));
        if (id == LOG_ID_ACCESS) log_write_file_access(buffer, (int)(ctx.pos - buffer));
    }
    return 0;
}

int log_init(void) {
    do {
        log_ctx = sys_alloc(sizeof(log_mgr_t));
        schk(NULL != log_ctx, break);

        ///O_APPEND make write is atomic operation
        log_ctx->log_fd_main = open(S5_PATH_LOG_FILE_MAIN, O_CREAT | O_RDWR | O_APPEND, 0644);
        if (log_ctx->log_fd_main < 0) {
            ahead_dbg("log main file open err. [%d]\n", errno);
            break;
        }
        
        log_ctx->log_fd_access = open(S5_PATH_LOG_FILE_ACCESS, O_CREAT | O_RDWR | O_APPEND, 0644);
        if (log_ctx->log_fd_access < 0) {
            ahead_dbg("log access file open err. [%d]\n", errno);
            break;
        }
        return 0;
    } while (0);

    if (log_ctx) {
        if (log_ctx->log_fd_main)
            close(log_ctx->log_fd_main);

        if (log_ctx->log_fd_access)
            close(log_ctx->log_fd_access);

        sys_free(log_ctx);
        log_ctx = NULL;
    }
    return -1;
}

int log_end(void) {
    if (log_ctx) {
        if (log_ctx->log_fd_main)
            close(log_ctx->log_fd_main);

        if (log_ctx->log_fd_access)
            close(log_ctx->log_fd_access);

        sys_free(log_ctx);
        log_ctx = NULL;
    }
    return 0;
}

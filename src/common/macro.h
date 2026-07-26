#ifndef _MACRO_H_INCLUDED_
#define _MACRO_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

#if __linux__
#define EVENT_EPOLL
#endif

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __linux__
#include <malloc.h>
#include <semaphore.h>
#elif __APPLE__
#include <dispatch/dispatch.h>
#include <sys/malloc.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#if defined(EVENT_EPOLL)
#include <sys/epoll.h>
#else
#include <sys/select.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>


#define MASK_SET(mask, bit) ({                \
    typeof(mask) __m = (mask);               \
    typeof(bit)  __b = (bit);                \
    __m |= __b;                              \
    __m;                                     \
})

#define MASK_CLR(mask, bit) ({                \
    typeof(mask) __m = (mask);               \
    typeof(bit)  __b = (bit);                \
    __m &= ~(__b);                           \
    __m;                                     \
})


/// for ahead log
#define ahead_dbg(format, ...) \
    do {  \
        fprintf(stderr, "[Ahead DBG]-%s:%d " format, __func__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0);

#define ahead_err(format, ...) \
    do {  \
        fprintf(stderr, "[Ahead ERR]-%s:%d " format, __func__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0);


/// open port limit
#define L_OPEN_PORT_MAX 64

// file paths
#define S5_PATH "/usr/local/s5/"
#define S5_PATH_LOG_DIR S5_PATH "logs/"
#define S5_PATH_LOG_FILE_MAIN S5_PATH_LOG_DIR "log_main"
#define S5_PATH_LOG_FILE_ACCESS S5_PATH_LOG_DIR "log_access"

#define S5_PATH_PID S5_PATH_LOG_DIR "pid"
#define S5_PATH_CFG S5_PATH "config/config.json"

// ASCII character hex number
#define CR 0xd
#define LF 0xa
#define SP 0x20

#define S5_SSL 0x1
#define S5_NOSSL 0x0

/// improve compile performance
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define schk(x, actions)                                                       \
    if (UNLIKELY(!(x))) {                                                      \
        err("schk err -> \"" #x "\".\n");                                      \
        actions;                                                               \
    }
#define sassert(x) schk(x, abort())

#define S5_OVER_TLS

enum connection_type {
    TYPE_TCP = 0x1,
    TYPE_UDP,
};

// http
enum http_process_status {
    HTTP_METHOD_GET = 1,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_CONNECT,
};

enum http_body_type {
    HTTP_BODY_TYPE_NULL = 1,
    HTTP_BODY_TYPE_CHUNK,
    HTTP_BODY_TYPE_CONTENT,
};

enum http_body_stat {
    HTTP_BODY_STAT_OK = 0x1,
    HTTP_BODY_STAT_DONE_CACHE = 0x2,
    HTTP_BODY_STAT_DONE_CACHENO = 0x4,
};

/* webser type */
enum webser_type {
    WEBSER_API = 1,
    WEBSER_FILE,
};

/* scoks5 module run model */
enum socks5_type {
    TLS_TUNNEL_C = 0x1,
    TLS_TUNNEL_S = 0x2,
    TLS_TUNNEL_S_SCRECT = 0x3,

};

// limits
enum limit_value {
    IPV4_LENGTH = 16,
    FILEPATH_LENGTH = 256,
    USERNAME_LENGTH = 16,
    PASSWD_LENGTH = 16,
    DOMAIN_LENGTH = 255
};

// statu types
enum status_value { OK = 0, ERROR = -1, AGAIN = -11, DONE = 1 };

// types
typedef volatile uint32_t atomic_t;
typedef struct net_connection_t con_t;
typedef int (*ev_cb)(con_t *c);

// macros
#define l_abs(x) (((x) >= 0) ? (x) : (-(x)))
#define l_unused(x) ((void)x)
#define l_strlen(str) ((uint32_t)strlen((char *)str))
#define l_min(x, y) (((x) < (y)) ? (x) : (y))
#define l_max(x, y) (((x) > (y)) ? (x) : (y))

#define ptr_get_struct(ptr, struct_type, struct_member)                        \
    ((struct_type *)(((unsigned char *)ptr) -  offsetof(struct_type, struct_member)))

typedef struct {
    ssize_t datan;
    char data[0];
} sys_data_t;

sys_data_t *sys_file_read_value(char *path);
int sys_file_write_data(char *fname, char *data, int datan);
int sys_file_exist(const char *fname);
int sys_directory_exist(const char *fpath);



#ifdef __cplusplus
}
#endif

#endif

#ifndef _MACRO_H_INCLUDED_
#define _MACRO_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

#if __linux__
#define EVENT_EPOLL
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#if __linux__
#include <malloc.h>
#include <semaphore.h>
#elif __APPLE__
#include <sys/malloc.h>
#include <dispatch/dispatch.h>
#endif
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#if defined(EVENT_EPOLL)
#include <sys/epoll.h>
#else
#include <sys/select.h>
#endif
#include <sys/mman.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sched.h>
#include <sys/resource.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>


/// for ahead tips
#define ahead_dbg(format, ...) printf( "[Ahead tips]-%s:%d "format, __func__, __LINE__, ##__VA_ARGS__)

/// open port limit
#define L_OPEN_PORT_MAX	64

// file paths
#define S5_PATH                 "/usr/local/s5/"
#define S5_PATH_LOG_DIR         S5_PATH"logs/"
#define S5_PATH_LOG_FILE_MAIN   S5_PATH_LOG_DIR"log_main"
#define S5_PATH_LOG_FILE_ACCESS S5_PATH_LOG_DIR"log_access"

#define S5_PATH_PID             S5_PATH_LOG_DIR"pid"
#define S5_PATH_CFG             S5_PATH"config/config.json"


// ASCII character hex number
#define CR	0xd
#define LF	0xa
#define SP	0x20


#define S5_SSL		0x1
#define S5_NOSSL    0x0


enum connection_type 
{
	TYPE_TCP = 0x1,
	TYPE_UDP,
};

// http
enum http_process_status
{
	HTTP_METHOD_GET        = 1,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
	HTTP_METHOD_CONNECT,
};

enum http_body_type
{
	HTTP_BODY_TYPE_NULL   = 1,
	HTTP_BODY_TYPE_CHUNK,
	HTTP_BODY_TYPE_CONTENT,
};

enum http_body_stat
{
	HTTP_BODY_STAT_OK = 0x1,
	HTTP_BODY_STAT_DONE_CACHE = 0x2,
	HTTP_BODY_STAT_DONE_CACHENO = 0x4,
};

/* webser type */
enum webser_type 
{
	WEBSER_API = 1,
	WEBSER_FILE,
};

/* scoks5 module run model */
enum socks5_type
{
    SOCKS5_CLIENT               = 0x1,
	SOCKS5_SERVER               = 0x2,
    SOCKS5_SERVER_SECRET        = 0x3,
	
};

// limits
enum limit_value {
	IPV4_LENGTH       =	16,
	FILEPATH_LENGTH   =	256,
    USERNAME_LENGTH   = 16,
	PASSWD_LENGTH     = 16,
	DOMAIN_LENGTH     =	255
};

// statu types
enum status_value {
	OK         = 0,
	ERROR      = -1,
	AGAIN      = -11,
	DONE       = 1
};

// types
typedef char                    byte;
typedef int32_t                 int32;
typedef int32_t                 status;
typedef uint32_t                uint32;
typedef volatile uint32         atomic_t;
typedef struct net_connection_t   con_t;

// macros
#define l_abs(x)                            (((x)>=0)?(x):(-(x)))
#define l_unused(x)                         ((void)x)
#define l_strlen(str)                       ((uint32)strlen((char*)str))
#define l_min(x,y)                          ((x<y)?x:y)
#define l_max(x,y)                          ((x>y)?x:y)
#define meta_len(start,end)                 ((uint32)l_max(0,((unsigned char*)end-(unsigned char*)start)))

#define sys_assert( x ) \
if( !(x) ) \
{ \
    printf("[assert] %s:%d ", __func__, __LINE__ );\
    printf("sys assert ->\""#x"\" false\n");\
    abort();\
}

#define ptr_get_struct( ptr, struct_type, struct_member ) \
(\
    (struct_type *)\
    (((unsigned char*)ptr)-offsetof(struct_type,struct_member))\
)

#ifdef __cplusplus
}
#endif


#endif

#ifndef _L_MACRO_H_INCLUDED_
#define _L_MACRO_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif


//#define EVENT_EPOLL

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
#include <sys/malloc.h>
#include <errno.h>
#include <semaphore.h>
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
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


// file paths
#define 	L_PATH_PREFIX				"/usr/local/lks5/"
#define 	L_PATH_PIDFILE				L_PATH_PREFIX"logs/pid"
#define 	L_PATH_CONFIG				L_PATH_PREFIX"config/config.json"
#define 	L_PATH_PERFTEMP				L_PATH_PREFIX"logs/l_perf"
#define 	L_PATH_LOG_MAIN				L_PATH_PREFIX"logs/l_log"
#define 	L_PATH_LOG_ACCESS			L_PATH_PREFIX"logs/l_access"
#define 	L_PATH_UOLOAD_FILE			L_PATH_PREFIX"logs/l_upload_temp"
#define 	L_OPEN_PORT_MAX				64

enum connection_type 
{
	TYPE_TCP		       = 0x1,
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
	HTTP_BODY_STAT_OK = 1,
	HTTP_BODY_STAT_DONE_CACHE = 3,
	HTTP_BODY_STAT_DONE_CACHENO = 5,
};

/* webser type */
enum webser_type 
{
	WEBSER_API = 1,
	WEBSER_STATIC,
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
typedef volatile uint32         l_atomic_t;
typedef struct l_connection_t   connection_t;

// macros
#define l_abs(x)                            (((x)>=0)?(x):(-(x)))
#define l_unused(x)                         ((void)x)
#define l_safe_free(x)                      (free(x))
#define l_safe_malloc(len)                  (malloc((uint32)(len)))
#define l_strlen(str)                       ((uint32)strlen((char*)str))
#define l_min(x,y)                          ((x<y)?x:y)
#define l_max(x,y)                          ((x>y)?x:y)
#define meta_len(start,end)                 ((uint32)l_max(0,((unsigned char*)end-(unsigned char*)start)))

#define l_get_struct( ptr, struct_type, struct_member ) \
(\
    (struct_type *)\
    (((unsigned char*)ptr)-offsetof(struct_type,struct_member))\
)

#ifdef __cplusplus
}
#endif


#endif

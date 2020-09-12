#ifndef _L_MACRO_H_INCLUDED_
#define _L_MACRO_H_INCLUDED_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <limits.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/select.h>
#include <malloc.h>
#include <errno.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <math.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


// file paths
#define 	L_PATH_PIDFILE			   "/usr/local/lks5/logs/pid"
#define 	L_PATH_CONFIG			   "/usr/local/lks5/config/config.json"
#define 	L_PATH_PERFTEMP			   "/usr/local/lks5/logs/l_perf"
#define 	L_PATH_LOG_MAIN			   "/usr/local/lks5/logs/l_log"
#define 	L_PATH_LOG_ACCESS		   "/usr/local/lks5/logs/l_access"
#define 	L_PATH_UOLOAD_FILE		   "/usr/local/lks5/logs/l_upload_temp"
#define 	L_OPEN_PORT_MAX			   64

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

	HTTP_PARSE_HEADER_DONE = 18,
	HTTP_PARSE_INVALID_HEADER
};

enum http_body_type
{
	HTTP_ENTITYBODY_NULL   = 1,
	HTTP_ENTITYBODY_CONTENT,
	HTTP_ENTITYBODY_CHUNK
};

#define HTTP_BODY_DONE 		0x01
#define HTTP_BODY_EMPTY		0x03
#define HTTP_BODY_RECVD		0x05

/* scoks5 module run model */
enum socks5_module_run_model {
	SOCKS5_SERVER,
	SOCKS5_CLIENT
};

// limits
enum limit_value {
	IPV4_LENGTH       =	16,
	FILEPATH_LENGTH   =	64,
	PASSWD_LENGTH     = 32,
	USERNAME_LENGTH   =	32,
	DOMAIN_LENGTH     =	64
};

// statu types
enum status_value {
	OK         = 0,
	ERROR      = -1,
	NOT_FOUND  = -2,
	AGAIN      = -18,
	DONE       = 2
};

// macros
#define l_abs(x)                          (((x)>=0)?(x):(-(x)))
#define l_unused( x )                     ((void)x)
#define l_safe_free( x )                  (free(x))
#define l_safe_malloc( len )              (malloc((size_t)(len)))
#define l_strlen( str )                   ((uint32)strlen( str ))
#define l_min( x, y )                     ( (x<y) ? x : y )
#define l_max( x, y )                     ( (x>y) ? x : y )
#define l_memcpy( dst, src, len )         ( memcpy((char*)dst, (char*)src, (size_t)len) );

#define l_get_struct( ptr, struct_type, struct_member ) \
(\
    (struct_type *)\
    ( ((char*)ptr) - offsetof( struct_type, struct_member ) )\
)

// types
typedef char                  byte;
typedef int32_t               int32;
typedef uint32_t              uint32;
typedef int32_t               status;
typedef volatile uint32       l_atomic_t;

typedef struct l_connection_t connection_t;

#endif

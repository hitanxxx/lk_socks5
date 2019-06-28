#ifndef _LK_GLOBAL_H_INCLUDED_
#define _LK_GLOBAL_H_INCLUDED_

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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
//
#define 	L_PATH_PIDFILE			"/usr/local/lksocks5/logs/pid"
#define 	L_PATH_CONFIG			"/usr/local/lksocks5/config/config.json"
#define 	L_PATH_PERFTEMP			"/usr/local/lksocks5/logs/l_perf"
#define 	L_PATH_LOG_MAIN			"/usr/local/lksocks5/logs/l_log"
#define 	L_PATH_LOG_ACCESS		"/usr/local/lksocks5/logs/l_access"
#define 	L_PATH_UOLOAD_FILE		"/usr/local/lksocks5/logs/l_upload_temp"

#define 	HTTP_METHOD_GET		0x000a
#define		HTTP_METHOD_HEAD	0x000b
#define 	HTTP_METHOD_POST	0x000c
#define 	HTTP_METHOD_PUT		0x000d
#define 	HTTP_METHOD_DELETE	0x000e
#define 	HTTP_METHOD_CONNECT	0x000f

#define OK 					0
#define ERROR 				-1
#define NOT_FOUND			-2
#define AGAIN 				-18
#define DONE				2

#define 	HTTP_PARSE_HEADER_DONE						14
#define 	HTTP_PARSE_INVALID_HEADER					13

#define l_abs(x)					(((x)>=0)?(x):(-(x)))
#define l_unused( x ) 			 	((void)x)
#define l_safe_free( x )			(free(x))
#define l_safe_malloc( len )		(malloc((size_t)(len)))
#define l_strlen( str )				((uint32)strlen( str ))
#define l_min( x, y )				( (x<y) ? x : y )
#define l_memcpy( dst, src, len ) \
	( memcpy((char*)dst, (char*)src, (size_t)len) );
#define l_get_struct( ptr, struct_type, struct_member ) \
(\
	(struct_type *)\
	( ((char*)ptr) - offsetof( struct_type, struct_member ) )\
)

typedef char  		byte;
typedef int32_t		int32;
typedef uint32_t	uint32;
typedef int32_t     status;
typedef volatile uint32	l_atomic_t;

typedef struct l_connection_t connection_t;

#endif

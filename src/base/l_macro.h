#ifndef _L_MACRO_H_INCLUDED_
#define _L_MACRO_H_INCLUDED_


// file paths
#define 	L_PATH_PIDFILE			"/usr/local/lks5/logs/pid"
#define 	L_PATH_CONFIG			"/usr/local/lks5/config/config.json"
#define 	L_PATH_PERFTEMP			"/usr/local/lks5/logs/l_perf"
#define 	L_PATH_LOG_MAIN			"/usr/local/lks5/logs/l_log"
#define 	L_PATH_LOG_ACCESS		"/usr/local/lks5/logs/l_access"
#define 	L_PATH_UOLOAD_FILE		"/usr/local/lks5/logs/l_upload_temp"

/* scoks5 module run model */
enum socks5_module_run_model {
	SOCKS5_SERVER,
	SOCKS5_CLIENT
};
// limits
enum limit_value {
	IPV4_LENGTH			=	16,
	FILEPATH_LENGTH		=	64,
	PASSWD_LENGTH		= 	32,
	USERNAME_LENGTH		=	32,
	DOMAIN_LENGTH		=	64
};

// statu types
enum status_value {
	OK			= 0,
	ERROR		= -1,
	NOT_FOUND	= -2,
	AGAIN		= -18,
	DONE		= 2
};
// macros
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

// types 
typedef char  						byte;
typedef int32_t						int32;
typedef uint32_t					uint32;
typedef int32_t     				status;
typedef volatile uint32				l_atomic_t;

typedef struct l_connection_t connection_t;

#endif

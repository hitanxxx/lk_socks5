#ifndef _USMGR_H_INCLUDE_
#define _USMGR_H_INCLUDE_ 

#ifdef __cplusplus
extern "C"
{
#endif

#define USER_NAME_MAX		16
#define USER_PASSWD_MAX		16
#define USER_AUTH_FILE_LEN  4096

typedef struct user_t {
	char		name[USER_NAME_MAX];
	char 		passwd[USER_PASSWD_MAX];

	queue_t		queue;
} user_t ;


status user_init(void);
status user_end(void);

status user_add( char * name, char * passwd );
status user_find( char * name, user_t ** user );

#ifdef __cplusplus
}
#endif
        
#endif

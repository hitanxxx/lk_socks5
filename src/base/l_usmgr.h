#ifndef _L_USMGR_H_INCLUDE_
#define _L_USMGR_H_INCLUDE_ 

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

status user_add( string_t * name, string_t * passwd );
status user_find( string_t * name, user_t ** user );


#endif

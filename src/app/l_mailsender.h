#ifndef _L_MAILSENDER_INCLUDE_
#define _L_MAILSENDER_INCLUDE_

#define MAIL_SENDER_MAX	1024

typedef struct mailsender
{	
	queue_t		queue;
	int 		sockfd;
	int 		tls;					// mail use tls encrypt 
	ssl_connection_t * 	ssl;
	
    char		host[32];				// mail server host
	int 		port;					// mail server port
	char		host_username[64];		// mail server username
	char		host_passwd[64];		// mail server passwd
	char 		from[64];				// mail from
	char		to[64];					// mail to
	char 		subject[32];			// mail subject
	char		context[128];			// mail context
} mailsender_t;

status mailsender_init( void );
status mailsender_exit(void);

status mailsender_alloc(   mailsender_t ** sender );
status mailsender_send(mailsender_t * sender);

#endif


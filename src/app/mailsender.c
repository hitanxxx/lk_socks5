#include "common.h"
#include "mailsender.h"
#include "ssl.h"

typedef struct mailsender_ctx
{
	queue_t			usable;
	queue_t			inuse;
	mailsender_t	pool[MAIL_SENDER_MAX];
} mailsender_ctx_t;

mailsender_ctx_t mail_ctx;

status mailsender_init(void)
{
	int i = 0;

	queue_init( &mail_ctx.usable );
	queue_init( &mail_ctx.inuse ); 
	
	for( i = 0; i < MAIL_SENDER_MAX; i ++ )
	{
		queue_insert_tail( &mail_ctx.usable, &mail_ctx.pool[i].queue );	
	}

	return OK;
}

status mailsender_exit(void)
{
	return OK;
}

static status mailsender_free( mailsender_t * sender )
{
	sender->sockfd = 0;
	memset( sender->host, 0, sizeof(sender->host) );
	sender->port = 0;
	memset( sender->host_username, 0, sizeof(sender->host_username) );
	memset( sender->host_passwd, 0, sizeof(sender->host_passwd) );
	memset( sender->from, 0, sizeof(sender->from) );
	memset( sender->to, 0, sizeof(sender->to) );
	memset( sender->context, 0, sizeof(sender->context) );
	
	queue_remove( &sender->queue );
	queue_insert_tail( &mail_ctx.usable, &sender->queue );

	return OK;
}

status mailsender_alloc( mailsender_t ** sender )
{
	mailsender_t * local_sender = NULL;
	queue_t * q = NULL;
	if( queue_empty( &mail_ctx.usable ) )
	{
		err("mailsender alloc, usable empty\n");
		return ERROR;
	}

	q = queue_head( &mail_ctx.usable );
	queue_remove( q );
	queue_insert_tail( &mail_ctx.inuse, q );

	local_sender = ptr_get_struct( q, mailsender_t, queue);

	*sender = local_sender;
	return OK;
}

status mailsender_recv(  mailsender_t * sender, char * buffer, int len )
{
	int rc = 0;

	if( sender->tls )
	{
		rc = SSL_read( sender->ssl->con, buffer, len );
	}
	else
	{
		rc = read( sender->sockfd, buffer, len );
	}
	return rc;
}

status mailsender_write( mailsender_t * sender, char * buffer, int len )
{
	int rc = 0;

	if( sender->tls )
	{
		rc = SSL_write( sender->ssl->con, buffer, len );
	}
	else
	{
		rc = send( sender->sockfd, buffer, len, 0 );
	}
	return rc;
}

#if(1)
/*
	mail sender use blocking io, detached thread to process
*/

static status mailsender_connection_close( mailsender_t * sender )
{
	if( sender->tls )
	{	
		if( sender->ssl )
		{
			SSL_shutdown( sender->ssl->con );
			SSL_free( sender->ssl->con );
			l_safe_free( sender->ssl );
		}
	}
	if( sender->sockfd )
	{
		close(sender->sockfd);
	}
	return OK;
}

static status mailsender_get_response(mailsender_t * sender)
{
	int ret = 0;
	char buffer[128] = {0};

	ret = mailsender_recv( sender, buffer, 128 );
    if( ret < 0 )
    {
        err("mailsender_get_response, recv failed [%d]\n", errno );
        return ERROR;
    }
	buffer[ret] = '\0';
	if( *buffer == '5' )
	{
		err("mailsender_get_response, the order not support smtp host\n");
		return ERROR;
	}
	if( sender->tls )
	{
		debug("tls get resp: [%s]\n", buffer );
	}
	else
	{
		debug("get resp: [%s]\n", buffer );
	}
	return OK;
}

static status mailsender_connect( mailsender_t * sender)
{
	struct sockaddr_in servaddr;
	sender->sockfd = socket(AF_INET,SOCK_STREAM,0);
    if( sender->sockfd < 0 )
    {
        err("mailsender_connect, create socket failed [%d]\n", errno );
        return ERROR;
    }

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family 	= AF_INET;
	servaddr.sin_port 		= htons(sender->port);

	if(inet_pton(AF_INET, sender->host, &servaddr.sin_addr) <= 0 ) 
    {
        err("mailsender_connect, inet_pton failed [%d]\n", errno );
        return ERROR;
    }

	debug("mailsender start connect...\n");
	if (connect( sender->sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0) 
    {
        err("mailsender_connect, connect failed [%d]\n", errno );
        return ERROR;
    }
	
	if( sender->tls )
	{
		int n = 0, sslerr = 0;
		sender->ssl = l_safe_malloc( sizeof(ssl_connection_t) );
		if( sender->ssl == NULL )
		{
			err("mailsender_connect, alloc ssl connection failed\n");
			return ERROR;
		}

		if( OK != ssl_load_ctx_certificate( &sender->ssl->session_ctx, L_SSL_CLIENT ) )
		{
			err("mailsender_connect, load ctx cert failed\n");
			return ERROR;
		}

		if( OK != ssl_load_con_certificate( sender->ssl->session_ctx, L_SSL_CLIENT, &sender->ssl->con ) )
		{
			err("mailsender_connect, load con ctx cert failed\n");
			return ERROR;
		}

		if( 0 == SSL_set_fd( sender->ssl->con, sender->sockfd ) )
		{
			err("mailsender_connect, set fd failed\n");
			return ERROR;
		}

		SSL_set_connect_state( sender->ssl->con );

		n =  SSL_connect( sender->ssl->con );
		if( 0 >= n )
		{
			sslerr = SSL_get_error(sender->ssl->con, n );
			ssl_record_error(sslerr);
			err("mailsender_connect, ssl connect failed\n");
			return ERROR;
		}
	}
	debug("mailsender connect success\n");

	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_connect, get response failed\n");
		return ERROR;
	}
	return OK;
}

static status mailsender_login( mailsender_t * sender )
{
	int ret = 0;
	char buffer[128] = {0};

	// send hello
	snprintf( buffer, sizeof(buffer), "HELO smtp.qq.com\r\n" );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_login, send HELO failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_login, send HELO get response failed\n");
		return ERROR;
	}

	// send auth login 
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "AUTH LOGIN\r\n" );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_login, send auth login failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_login, send auth login get response failed\n");
		return ERROR;
	}

	// send username 
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "%s\r\n", sender->host_username );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_login, send auth username failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_login, send auth username get response failed\n");
		return ERROR;
	}

	// send passwd
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "%s\r\n", sender->host_passwd );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_login, send auth passwd failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_login, send auth passwd get response failed\n");
		return ERROR;
	}
	return OK;
}

static status mailsender_data( mailsender_t * sender )
{
	int ret = 0;
	char buffer[1024] = {0};

	// send from
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "MAIL FROM:<%s>\r\n", sender->from );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_data, send from failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_data, send from get response failed\n");
		return ERROR;
	}

	// send to 
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "RCPT TO:<%s>\r\n", sender->to );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_data, send to failed\n");
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_data, send to get response failed\n");
		return ERROR;
	}

	// send data
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "DATA\r\n" );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_data, send data failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_data, send data get response failed\n");
		return ERROR;
	}

	// send context
	int len = 0;
	memset( buffer, 0, sizeof(buffer) );
	len += snprintf( buffer + len, sizeof(buffer) - len, "From: %s\r\n", sender->from );
	len += snprintf( buffer + len, sizeof(buffer) - len, "To: %s\r\n", sender->to );
	len += snprintf( buffer + len, sizeof(buffer) - len, "Subject: %s\r\n", sender->subject );
	len += snprintf( buffer + len, sizeof(buffer) - len, "\r\n\r\n" );
	len += snprintf( buffer + len, sizeof(buffer) - len, "%s", sender->context );
	len += snprintf( buffer + len, sizeof(buffer) - len, "\r\n.\r\n" );
	ret = mailsender_write( sender, buffer, strlen(buffer));
    if( ret < 0 )
    {
    	err("mailsender_data, send context failed [%d]\n", errno );
        return ERROR;
    }
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_data, send context get response failed\n");
		return ERROR;
	}

	// send quit
	memset( buffer, 0, sizeof(buffer) );
	snprintf( buffer, sizeof(buffer), "QUIT\r\n" );
	ret = mailsender_write( sender, buffer, strlen(buffer));
	if( ret < 0 )
	{
		err("mailsender_data, send quit failed [%d]\n", errno );
		return ERROR;
	}
	if( mailsender_get_response(sender) < 0 )
	{
		err("mailsender_data, send quit get response failed\n");
		return ERROR;
	}
	return OK;
}

static void * mailsender_workthr( void * para )
{
	mailsender_t * sender = ( mailsender_t * )para;
	do 
	{
		if( OK != mailsender_connect( sender ) )
		{
			err("mailsender_connect failed\n");
			break;
		}

		if( OK != mailsender_login( sender ) )
		{
			err("mailsender_login failed\n");
			break;
		}

		if( OK != mailsender_data( sender ) )
		{
			err("mailsender_data failed\n");
			break;
		}
	}
	while(0);

	mailsender_connection_close( sender );
	mailsender_free( sender );
	return NULL;
}


status mailsender_send( mailsender_t * sender )
{
	int rc = 0;
	
	pthread_t thrid;
	pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, 2*1024*1024);

	rc = pthread_create( &thrid, &attr, mailsender_workthr, sender );
	if( rc )
	{
		err("mailsender create thread to process failed [%d]\n", errno );
		return ERROR;
	}
	pthread_attr_destroy(&attr);
	return OK;
}
#endif


#include "common.h"



static queue_t g_users;
static mem_page_t * g_user_mempage = NULL;


status user_find( char * name, user_t ** user )
{
	queue_t * q;
	user_t * t = NULL;

	for( q = queue_head( &g_users ); q != queue_tail( &g_users ); q = queue_next(q) )
	{
		t = ptr_get_struct( q, user_t, queue );
		if( t && l_strlen(t->name) == strlen(name) && memcmp( t->name, name, strlen(name) ) == 0 )
		{
			if( user )
			{
				*user = t;
			}
			return OK;
		}
	}
	return ERROR;
}

status user_add( char * name, char * passwd )
{
	user_t * user = NULL;

	user = mem_page_alloc( g_user_mempage, sizeof(user_t) );
	if( !user )
	{
		err("alloc new user\n");
		return ERROR;
	}
	memset( user, 0, sizeof(user_t) );

	memcpy( user->name, name, strlen(name) );
	memcpy( user->passwd, passwd, strlen(passwd) );
	queue_insert_tail( &g_users, &user->queue );

#if(1)
	// show all users
	queue_t * q;
	user_t * t = NULL;
	debug("===\n");
	for( q = queue_head( &g_users ); q != queue_tail( &g_users ); q = queue_next(q) )
	{
		t = ptr_get_struct( q, user_t, queue );
		debug("queue show [%s] --- [%s]\n", t->name, t->passwd );
	}
#endif
	return OK;
}

static status usmgr_auth_file_decode( meta_t * meta )
{
	cJSON * socks5_user_database = NULL;

	cJSON * root = cJSON_Parse( (char*)meta->pos );
	if( root )
	{
		socks5_user_database = cJSON_GetObjectItem( root, "socks5_user_database" );
		if( socks5_user_database )
		{
			int i = 0;
			cJSON * obj = NULL;
			cJSON * username = NULL;
			cJSON * passwd = NULL;
			for( i = 0; i < cJSON_GetArraySize( socks5_user_database ); i ++ )
			{
				obj = cJSON_GetArrayItem( socks5_user_database, i );
				if( obj )
				{
					username = cJSON_GetObjectItem( obj, "username" );
					passwd = cJSON_GetObjectItem( obj, "passwd" );

					if( username && passwd )
					{
						user_add( cJSON_GetStringValue(username), cJSON_GetStringValue(passwd) );
					}
				}
			}
		}
		cJSON_Delete( root );
	}
    return OK;
}

static status usmgr_auth_file_load( meta_t * meta )
{
    int fd = 0;
    ssize_t size = 0;
    
    fd = open( (char*)config_get()->s5_serv_auth_path, O_RDONLY  );
    if( ERROR == fd )
    {
        err("usmgr auth open file [%s] failed, errno [%d]\n", config_get()->s5_serv_auth_path,  errno );
        return ERROR;
    }
    size = read( fd, meta->pos, meta_len( meta->start, meta->end ) );
    close( fd );
    if( size == ERROR )
    {
        err("usmgr auth read auth file failed\n");
        return ERROR;
    }
    meta->last += size;
    return OK;
}

static status user_datebase_init( void )
{
    meta_t * meta = NULL;
    status rc = ERROR;
    
    do
    {
        if( OK != meta_alloc( &meta, USER_AUTH_FILE_LEN ) )
        {
            err("usmgr auth databse alloc meta failed\n");
            break;
        }
        
        if( OK != usmgr_auth_file_load( meta ) )
        {
            err("usmgr auth file load failed\n");
            break;
        }
        
        if( OK != usmgr_auth_file_decode( meta ) )
        {
            err("usmgr auth file decode failed\n");
            break;
        }
        rc = OK;
    }while(0);
    
    if( meta )
    {
        meta_free( meta );
    }
    return rc;
}

status user_init(  void)
{
	queue_init( &g_users );
	if( OK != mem_page_create( &g_user_mempage, sizeof(user_t) ) )
	{
		err("usmgr alloc user mem page\n");
		return ERROR;
	}
    if( OK != user_datebase_init() )
    {
        err("usmgr auth database init failed\n");
        return ERROR;
    }
	return OK;
}

status user_end(void)
{
	if( g_user_mempage )
	{
		mem_page_free( g_user_mempage );
	}
	return OK;
}


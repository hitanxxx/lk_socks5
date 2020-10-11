#include "l_base.h"


static queue_t g_users;
static l_mem_page_t * g_user_mempage = NULL;


status user_find( string_t * name, user_t ** user )
{
	queue_t * q;
	user_t * t = NULL;

	for( q = queue_head( &g_users ); q != queue_tail( &g_users ); q = queue_next(q) )
	{
		t = l_get_struct( q, user_t, queue );
		if( t && memcmp( t->name, name->data, name->len ) == 0 && l_strlen(t->name) == name->len )
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

status user_add( string_t * name, string_t * passwd )
{
	user_t * user = NULL;

	user = l_mem_alloc( g_user_mempage, sizeof(user_t) );
	if( !user )
	{
		err("alloc new user\n");
		return ERROR;
	}
	memset( user, 0, sizeof(user_t) );

	memcpy( user->name, name->data, name->len );
	memcpy( user->passwd, passwd->data, passwd->len );
	queue_insert_tail( &g_users, &user->queue );

#if(1)
	// show all users
	queue_t * q;
	user_t * t = NULL;

	for( q = queue_head( &g_users ); q != queue_tail( &g_users ); q = queue_next(q) )
	{
		t = l_get_struct( q, user_t, queue );
		debug("queue show [%s] --- [%s]\n", t->name, t->passwd );
	}
#endif
	return OK;
}

static status usmgr_auth_file_decode( meta_t * meta )
{
    ljson_ctx_t * ctx = NULL;
    ljson_node_t * root_obj, *db_arr, *db_index;
    ljson_node_t * username, *userpasswd;
    status rc = OK;
    
    if( OK != json_ctx_create( &ctx ) )
    {
        err("usmgr auth json ctx create\n");
        return ERROR;
    }
    if( OK !=  json_decode( ctx, meta->pos, meta->last ) )
    {
        err("usmgr auth json decode\n");
        rc = ERROR;
        goto failed;
    }
    if( OK !=  json_get_child( &ctx->root, 1, &root_obj ) )
    {
        err("usmgr auth json get root child\n");
        rc = ERROR;
        goto failed;
    }
    if( OK != json_get_obj_child_by_name( root_obj, "socks5_user_database", l_strlen("socks5_user_database"), &db_arr) )
    {
        err("usmgr auth json get database arr\n");
        rc = ERROR;
        goto failed;
    }
    db_index = db_arr->child;
    while( db_index )
    {
        if( OK != json_get_obj_child_by_name( db_index, "username", l_strlen("username"), &username ) )
        {
            err("usmgr auth json array child find username\n");
            rc = ERROR;
            goto failed;
        }
        if( OK != json_get_obj_child_by_name( db_index, "passwd", l_strlen("passwd"), &userpasswd ) )
        {
            err("usmgr auth json array child find username\n");
            rc = ERROR;
            goto failed;
        }
        
        debug("usmgr auth info [%.*s]:[%.*s]\n", username->name.len, username->name.data, userpasswd->name.len, userpasswd->name.data );
        user_add( &username->name, &userpasswd->name );
        db_index = db_index->next;
    }
failed:
    json_ctx_free( ctx );
    return rc;
}

static status usmgr_auth_file_load( meta_t * meta )
{
    int fd = 0;
    ssize_t size = 0;
    
    fd = open( (char*)conf.base.authfile, O_RDONLY  );
    if( ERROR == fd )
    {
        err("usmgr auth open file [%s] failed, errno [%d]\n", conf.base.authfile,  errno );
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
	if( OK != l_mem_page_create( &g_user_mempage, sizeof(user_t) ) )
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
	return OK;
}


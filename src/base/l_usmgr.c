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
		if( t && strncmp( t->name, name->data, name->len ) == 0 && l_strlen(t->name) == name->len )
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

	strncpy( user->name, name->data, name->len );
	strncpy( user->passwd, passwd->data, passwd->len );
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

status user_init(  void)
{
	queue_init( &g_users );
	if( OK != l_mem_page_create( &g_user_mempage, sizeof(user_t) ) )
	{
		err("alloc user mem page\n");
		return ERROR;
	}
	return OK;
}

status user_end(void)
{
	return OK;
}


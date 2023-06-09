#include "common.h"
#include "queue.h"

typedef struct
{
#if defined(EVENT_EPOLL)
    int32           event_fd;
    struct epoll_event * events;
#else
    fd_set          rfds;
    fd_set          wfds;
#endif
    event_handler_t g_event_handler;
    queue_t         usable;
    queue_t         use;
	int				queue_use_num;
    event_t         pool[0];
} g_event_t;
static g_event_t * g_event_ctx = NULL;


#if defined(EVENT_EPOLL)
static status event_epoll_init(     )
{
	g_event_ctx->events = (struct epoll_event*) l_safe_malloc ( sizeof(struct epoll_event)*MAX_NET_CON );
	if( NULL == g_event_ctx->events ) {
		err("ev epoll malloc events pool failed\n" );
		return ERROR;
	}
	g_event_ctx->event_fd = epoll_create1(0);
	if( g_event_ctx->event_fd == ERROR ) {
		err("ev epoll open event fd faield, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

static status event_epoll_end( )
{
	if( g_event_ctx->event_fd ) {
		close( g_event_ctx->event_fd );
	}
	if( g_event_ctx->events ) {
		l_safe_free( g_event_ctx->events );
	}
	return OK;
}

static status event_epoll_opt( event_t * ev, int32 fd, int trigger_type )
{
	struct epoll_event sysev, *p_sysev = NULL;
	int sysop = 0;

	memset( &sysev, 0, sizeof(struct epoll_event) );

	/// want type not same as record type
	if( ev->trigger_type_record != trigger_type ) {
		 
		if( trigger_type == EV_NONE ) {
			sysop = EPOLL_CTL_DEL;
			p_sysev = NULL;
			ev->f_active = 0;
		} else {
			if( ev->trigger_type_record == EV_NONE ) {
				sysop = EPOLL_CTL_ADD;
			} else {
				sysop = EPOLL_CTL_MOD;
			}
			sysev.data.ptr = (void*)ev;
			sysev.events = EPOLLET|trigger_type;
			p_sysev = &sysev;
			ev->f_active = 1;
		}
	
		if( OK != epoll_ctl( g_event_ctx->event_fd, sysop, fd, p_sysev ) ) {
			err("epoll ctrl fd [%d] error sysop [%d] trigger_type [%x], errno [%d] [%s]\n", fd, sysop, trigger_type, errno, strerror(errno) );
			return ERROR;
		}

		if( !ev->fd ) ev->fd	= fd;
		ev->trigger_type_record = trigger_type;
	}
	return OK;
}

status event_epoll_run( time_t msec )
{
	int32 i = 0, act_num = 0;
    event_t * ev = NULL;
	uint32 trigger_type = 0;

	act_num = epoll_wait( g_event_ctx->event_fd, g_event_ctx->events, MAX_NET_CON, (int)msec );
    systime_update( );
	if( act_num <= 0 ) {
		if( act_num < 0 ) {
			if( errno == EINTR ) {
				err("evt epoll_wait interrupt by signal\n");
				return OK;
			}
			err("ev epoll wait failed, [%d]", errno );
			return ERROR;
		}
		if( msec != -1 ) {
			return OK;
		}
		err("ev epoll return 0\n");
		return ERROR;
	}

	for( i = 0; i < act_num; i ++ )  {
		ev = g_event_ctx->events[i].data.ptr;
		trigger_type = g_event_ctx->events[i].events;
        if( ev->f_active ) {
            if( (trigger_type & EV_R) && ev->read_pt )
                ev->read_pt( ev );
            
            if( (trigger_type & EV_W) && ev->write_pt )
                ev->write_pt( ev );
        }
	}
	return OK;
}
#else


static status event_select_init( void )
{
	FD_ZERO( &g_event_ctx->rfds );
	FD_ZERO( &g_event_ctx->wfds );
	return OK;
}

static status event_select_end( void )
{
    FD_ZERO( &g_event_ctx->rfds );
    FD_ZERO( &g_event_ctx->wfds );
	return OK;
}

static status event_select_opt( event_t * ev, int32 fd, int trigger_type )
{
	if( ev->trigger_type_record != trigger_type ) {
		if ( (trigger_type & EV_R) && ( !(trigger_type & EV_W) ) ) {
			if( ev->trigger_type_record & EV_W )
				FD_CLR( fd, &g_event_ctx->wfds );

			if( !( ev->trigger_type_record & EV_R) )
				FD_SET( fd, &g_event_ctx->rfds );
		}
		else if ( (trigger_type & EV_W) && ( !(trigger_type & EV_R) ) ) {
			if( ev->trigger_type_record & EV_R )
				FD_CLR( fd, &g_event_ctx->rfds );
			
			if( !( ev->trigger_type_record & EV_W ) )
				FD_SET( fd, &g_event_ctx->wfds );
		}
		else if( (trigger_type & EV_R) && (trigger_type & EV_W) ) {
			if( !(ev->trigger_type_record & EV_R) )
				FD_SET( fd, &g_event_ctx->rfds );

			if( !(ev->trigger_type_record & EV_W) )
				FD_SET( fd, &g_event_ctx->wfds );
		}
		else if ( trigger_type == EV_NONE ) {
			if( ev->trigger_type_record & EV_W )
				FD_CLR( fd, &g_event_ctx->wfds );

			if( ev->trigger_type_record & EV_R )
				FD_CLR( fd, &g_event_ctx->rfds );
		}

		if( trigger_type == EV_NONE ) {
			ev->f_active = 0;
		} else {
			if( !ev->f_active ) ev->f_active = 1;
		}
		if( 0 == ev->fd ) 
			ev->fd = fd;
		ev->trigger_type_record = trigger_type;
	}
	return OK;
}

status event_select_run( time_t msec )
{
	struct timeval tv, *p_tv = NULL;
	int i = 0, fd_max = -1;
	int act_num = 0, act_cnt = 0;
	
	fd_set tmp_rfds;
	fd_set tmp_wfds;

	memset( &tv, 0, sizeof(struct timeval) );
	if( msec != -1 ) {
		tv.tv_sec 	= msec/1000;
		tv.tv_usec	= (msec%1000)*1000;
		p_tv = &tv;
	}
	
	/// find max fd in listen events
	for( i = 0; i < listens->elem_num; i ++ ) {
		listen_t * p_listen = mem_arr_get( listens, i + 1 );
		event_t * p_ev = &p_listen->event;
		if( p_ev->f_active == 1 && p_ev->fd > fd_max ) {
			fd_max = p_ev->fd;
		}
	}
	/// find max fd in pool events 
	for( i = 0; i < MAX_NET_CON; i++ ) {
		if( g_event_ctx->pool[i].f_active == 1 && g_event_ctx->pool[i].fd > fd_max ) {
			fd_max = g_event_ctx->pool[i].fd;
		}
	}
	

	// select return will be change read fds and write fds
	memcpy( &tmp_rfds, &g_event_ctx->rfds, sizeof(fd_set) );
	memcpy( &tmp_wfds, &g_event_ctx->wfds, sizeof(fd_set) );

	act_num = select( fd_max + 1, &tmp_rfds, &tmp_wfds, NULL, p_tv );
	systime_update( );
	if( act_num <= 0 ) {
		if( act_num < 0 ) {
			if( errno == EINTR ) {
				err("evt select interrupt by signal\n");
				return OK;
			}
			err("ev select wait failed, [%d]\n", errno );
			return ERROR;
		}
		if( msec != -1 ) {
			/// wait timeout
			return OK;
		}
		err("ev select return 0\n");
		return ERROR;
	}
	
	
	/// loop check all listen events
	for( i = 0; i < listens->elem_num; i ++ ) {
		int faction = 0;
		listen_t * p_listen = mem_arr_get( listens, i + 1 );
		event_t * p_ev = &p_listen->event;
		if( p_ev->f_active == 1 ) {
			if( FD_ISSET( p_ev->fd, &tmp_rfds ) && p_ev->read_pt ) {
				p_ev->read_pt(p_ev);
				faction |= 0x1;
			}
			if( FD_ISSET( p_ev->fd, &tmp_wfds ) && p_ev->write_pt ) {
				p_ev->write_pt(p_ev);
				faction |= 0x2;
			}
			if( faction > 0 ) act_cnt ++;
			if( act_cnt >= act_num ) break;
		}
	}

	/// loop check all events 
	for( i = 0; i < MAX_NET_CON; i ++ ) {
		int faction = 0;
		event_t * p_ev = &g_event_ctx->pool[i];
		if( p_ev->f_active == 1 ) {
			if( FD_ISSET( p_ev->fd, &tmp_rfds ) && p_ev->read_pt ) {
				p_ev->read_pt(p_ev);
				faction |= 0x1;
			}
			if( FD_ISSET( p_ev->fd, &tmp_wfds ) && p_ev->write_pt ) {
				p_ev->write_pt(p_ev);
				faction |= 0x2;
			}
			if( faction > 0 ) act_cnt ++;
			if( act_cnt >= act_num ) break;
		}
	}
	return OK;
}
#endif

status event_opt( event_t * event, int32 fd, int trigger_type )
{
	return g_event_ctx->g_event_handler.opt( event, fd, trigger_type );
}

status event_run( time_t msec )
{	
	int i = 0;
	listen_t * listen = NULL;

	/// if process current used event num < all*80%, then try to listen
	if( g_event_ctx->queue_use_num <= (MAX_NET_CON*80/100) ){
		// try to get listen event lock
		process_lock();
		if( process_mutex_value_get() == 0 ) {
			for( i = 0; i < listens->elem_num; i ++ )  {
				listen = mem_arr_get( listens, i+1 );
				listen->event.data 		= listen;
				listen->event.read_pt 	= net_accept;
				event_opt( &listen->event, listen->fd, EV_R );
			}
			process_mutex_value_set( proc_pid() );	
		}
		process_unlock();
	}

	g_event_ctx->g_event_handler.run( msec );
	
	process_lock();
	if( process_mutex_value_get() == proc_pid() ) {
		for( i = 0; i < listens->elem_num; i ++ )  {
			listen = mem_arr_get( listens, i+1 );
			listen->event.data 		= listen;
			listen->event.read_pt 	= net_accept;
			event_opt( &listen->event, listen->fd, EV_NONE );
		}	
		process_mutex_value_set(0);
	}
	process_unlock();
	
	return 0;
}

status event_alloc( event_t ** ev )
{
	queue_t * q = NULL;
	event_t * local_ev = NULL;
	if( 1 == queue_empty( &g_event_ctx->usable ) ) {
		err("event usable empty\n");
		return ERROR;
	}
	q = queue_head( &g_event_ctx->usable );
	queue_remove( q );
	queue_insert_tail( &g_event_ctx->use, q );
	g_event_ctx->queue_use_num ++;
	local_ev = ptr_get_struct( q, event_t, queue);
	
	*ev = local_ev;
	return OK;
}

status event_free( event_t * ev )
{
	if( ev ) {
		if( ev->f_active && ev->trigger_type_record != EV_NONE )  {
			event_opt( ev, ev->fd, EV_NONE );
		}
		ev->trigger_type_record = 0;
		
		ev->fd			= 0;
		ev->read_pt		= NULL;
		ev->write_pt	= NULL;
		ev->data		= NULL;
		
		timer_del( &ev->timer );
		ev->f_active	= 0;
		
		queue_remove( &ev->queue );
		queue_insert_tail( &g_event_ctx->usable, &ev->queue );
		g_event_ctx->queue_use_num --;
	}
	return OK;
}

status event_init( void )
{	
	if( g_event_ctx ) {
		err("g_event_ctx not empty\n");
		return ERROR;
	}
	g_event_ctx = l_safe_malloc( sizeof(g_event_t) + (sizeof(event_t)*MAX_NET_CON) );
	if( !g_event_ctx ) {
		err("event alloc this failed, [%d]\n", errno );
		return ERROR;
	}
	memset( g_event_ctx, 0, sizeof(g_event_t) + (sizeof(event_t)*MAX_NET_CON) );
	
	queue_init( &g_event_ctx->use );
	queue_init( &g_event_ctx->usable );
	for( int i = 0; i < MAX_NET_CON; i ++ ) {
		queue_insert_tail( &g_event_ctx->usable, &g_event_ctx->pool[i].queue );
	}

#if defined(EVENT_EPOLL)
    g_event_ctx->g_event_handler.init      = event_epoll_init;
    g_event_ctx->g_event_handler.end       = event_epoll_end;
    g_event_ctx->g_event_handler.opt       = event_epoll_opt;
    g_event_ctx->g_event_handler.run       = event_epoll_run;
#else
    g_event_ctx->g_event_handler.init      = event_select_init;
    g_event_ctx->g_event_handler.end       = event_select_end;
    g_event_ctx->g_event_handler.opt       = event_select_opt;
    g_event_ctx->g_event_handler.run       = event_select_run;
#endif

	// init event, and add listen into event
	if( g_event_ctx->g_event_handler.init ) {
		g_event_ctx->g_event_handler.init();
	}
	return OK;
	
}

status event_end( void )
{
	if( g_event_ctx ) {
	    int i = 0;
        listen_t * listen = NULL;
	    process_lock();
    	if( process_mutex_value_get() == proc_pid() ) {
    		for( i = 0; i < listens->elem_num; i ++ )  {
    			listen = mem_arr_get( listens, i+1 );
    			listen->event.data 		= listen;
    			listen->event.read_pt 	= net_accept;
    			event_opt( &listen->event, listen->fd, EV_NONE );
    		}	
    		process_mutex_value_set(0);
    	}
    	process_unlock();
    	
		if( g_event_ctx->g_event_handler.end ) {
			g_event_ctx->g_event_handler.end();
		}
		l_safe_free( g_event_ctx );
		g_event_ctx = NULL;
	}

	return OK;
}

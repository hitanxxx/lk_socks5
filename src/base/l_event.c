#include "l_base.h"

typedef struct private_event
{
#if defined(EVENT_EPOLL)
    int32           event_fd;
    struct epoll_event * events;
#else
    fd_set          rfds;
    fd_set          wfds;
    fd_set          refds;
    fd_set          wefds;
    int32           g_maxfd;
    event_t*        ev_arr[MAX_NET_CON];
#endif
    event_handler_t g_event_handler;
    queue_t         usable;
    queue_t         use;
    event_t         pool[0];
} private_event_t;
static private_event_t * this = NULL;


#if defined(EVENT_EPOLL)
static status event_epoll_init(     )
{
	this->events = (struct epoll_event*) l_safe_malloc ( sizeof(struct epoll_event)*MAX_NET_CON );
	if( NULL == this->events )
	{
		err("ev epoll malloc events pool failed\n" );
		return ERROR;
	}
	this->event_fd = epoll_create1(0);
	if( this->event_fd == ERROR )
	{
		err("ev epoll open event fd faield, [%d]\n", errno );
		return ERROR;
	}
	return OK;
}

static status event_epoll_end( )
{
	if( this->event_fd )
	{
		close( this->event_fd );
	}
	if( this->events )
	{
		l_safe_free( this->events );
	}
	return OK;
}

static status event_epoll_opt( event_t * ev, int32 fd, net_event_type trigger_type )
{
	struct epoll_event opt_ev, *p_opt_ev = NULL;
	int32	opt_type = 0;

	memset( &opt_ev, 0, sizeof(struct epoll_event) );
	ev->type = trigger_type;
	do 
	{
		if( (trigger_type & EV_R) && (trigger_type & EV_W) )
		{
			if( ev->f_read && ev->f_write ) break;
			opt_type = ( !ev->f_read && !ev->f_write ) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
		}
		else if( trigger_type & EV_R )
		{ 
			if( !ev->f_write && ev->f_read ) break;
			else if( ev->f_write )
			{
				opt_type = EPOLL_CTL_MOD;
				ev->f_write = 0;
			}
			else if ( !ev->f_write && !ev->f_read ) opt_type = EPOLL_CTL_ADD;
		}
		else if( trigger_type & EV_W )
		{
			if( !ev->f_read && ev->f_write ) break;
			else if ( ev->f_read ) 
			{
				opt_type = EPOLL_CTL_MOD;
				ev->f_read = 0;	
			}
			else if ( !ev->f_read && !ev->f_write ) opt_type = EPOLL_CTL_ADD;
		}
		else if ( trigger_type == EV_NONE )
		{
			opt_type = EPOLL_CTL_DEL;
			trigger_type = EV_NONE;
			ev->f_read = ev->f_write = 0;
		}
		
		if( trigger_type & EV_R ) ev->f_read 	= 1;
		if( trigger_type & EV_W ) ev->f_write 	= 1;

		if( trigger_type == EV_NONE )
		{
			p_opt_ev = NULL;
		}
		else
		{
			opt_ev.data.ptr	= (void*)ev;
			opt_ev.events 	= EPOLLET|trigger_type;
			p_opt_ev = &opt_ev;
		}
		if( OK != epoll_ctl( this->event_fd, opt_type, fd, p_opt_ev ) )
		{
			err("event epoll ctrl [%d] failed, [%d]\n", fd, errno );
			return ERROR;
		}
		ev->f_active 			= 1;
		if( !ev->fd ) ev->fd 	= fd;
		return OK;
	} while(0);
	return OK;
}

status event_epoll_run( time_t msec )
{
	int32 i = 0, num = 0;
    event_t * ev = NULL;
	uint32 ev_type = 0;

	num = epoll_wait( this->event_fd, this->events, MAX_NET_CON, (int)msec );
	l_time_update( );
	if( num <= 0 )
	{
		if( num < 0 )
		{
			if( errno == EINTR )
			{
				err("ev epoll_wait stop by signal\n");
				return OK;
			}
			err("ev epoll wait failed, [%d]", errno );
			return ERROR;
		}
		if( msec != -1 )
		{
			return OK;
		}
		err("ev epoll return 0\n");
		return ERROR;
	}

	for( i = 0; i < num; i ++ ) 
	{
		ev 		    = this->events[i].data.ptr;
		ev_type 	= this->events[i].events;
        if( ev->f_active )
        {
            if( (ev_type & EV_R) && ev->f_read && ev->read_pt )
            {
                ev->read_pt( ev );
            }
            if( (ev_type & EV_W) && ev->f_write && ev->write_pt )
            {
                ev->write_pt( ev );
            }
        }
	}
	return OK;
}
#else
static uint32 event_select_position_get()
{
    int i = 0;
	for( i = 0; i < MAX_NET_CON; i ++ )
	{
		if( NULL == this->ev_arr[i] )
        {
            return i;
        }
	}
	return ERROR;
}

static status event_select_position_free( uint32 index )
{
	if( NULL == this->ev_arr[index] )
	{
		err("ev select arr idx [%d] already clear\n", index );
		return ERROR;
	}
	this->ev_arr[index]   = NULL;
	return OK;
}

static status event_select_init( void )
{
	FD_ZERO( &this->rfds );
	FD_ZERO( &this->wfds );
	FD_ZERO( &this->refds );
	FD_ZERO( &this->wefds );
	this->g_maxfd = 0;
	return OK;
}

static status event_select_end( void )
{
    FD_ZERO( &this->rfds );
    FD_ZERO( &this->wfds );
    FD_ZERO( &this->refds );
    FD_ZERO( &this->wefds );
    this->g_maxfd = 0;
	return OK;
}

static status event_select_opt( event_t * ev, int32 fd, net_event_type trigger_type )
{
	ev->type =	trigger_type;
	do 
	{
		if( (trigger_type & EV_R) && (trigger_type & EV_W) )
		{
			if( ev->f_read && ev->f_write ) break;
			if( !ev->f_read )
			{
				FD_SET( fd, &this->rfds );
			}
			if( !ev->f_write )
			{
				FD_SET( fd, &this->wfds );
			}
		}
		else if ( trigger_type & EV_R )
		{
			if( ev->f_write )
			{
				FD_CLR( fd, &this->wfds );
				ev->f_write = 0;
			}
			if( ev->f_read ) break;
			FD_SET( fd, &this->rfds );
		}
		else if ( trigger_type & EV_W )
		{
			if( ev->f_read )
			{
				FD_CLR( fd, &this->rfds );
				ev->f_read = 0;
			}
			if( ev->f_write ) break;
			FD_SET( fd, &this->wfds );
		}
		else if ( trigger_type == EV_NONE )
		{
			if( ev->f_read )
			{
				FD_CLR( fd, &this->rfds );
				ev->f_read = 0;
			}
			if( ev->f_write )
			{
				FD_CLR( fd, &this->wfds );
				ev->f_write = 0;
			}
		}
	} while(0);
	
	if( trigger_type & EV_R ) ev->f_read 	= 1;
	if( trigger_type & EV_W ) ev->f_write 	= 1;
	
	if( !ev->f_active )
	{
		ev->idx = event_select_position_get();
		if( ERROR == ev->idx )
		{
			err("ev select arr no position\n");
			return ERROR;
		}
		this->ev_arr[ev->idx] 	= ev;
        ev->f_position          = 1;
		
		if( !ev->fd ) ev->fd 	= fd;
		ev->f_active  = 1;
	}
	return OK;
}

status event_select_run( time_t msec )
{
	struct timeval s_tv, * tv;
	status num = 0;
	int32 i = 0;

	memset( &s_tv, 0, sizeof(struct timeval) );
	if( msec != -1 )
	{
		s_tv.tv_sec 	= msec/1000;
		s_tv.tv_usec	= (msec%1000)*1000;
		tv = &s_tv;
	}
	else
	{
		tv = NULL;
	}
	
	this->g_maxfd = -1;
	for( i = 0; i < MAX_NET_CON; i ++ )
	{
 		if( this->ev_arr[i] )
 		{
			if( this->ev_arr[i]->fd > this->g_maxfd )
			{
				this->g_maxfd = this->ev_arr[i]->fd;
			}
		}
	}
	this->refds	= this->rfds;
	this->wefds	= this->wfds;
	num = select( this->g_maxfd+1, &this->refds, &this->wefds, NULL, tv );
	l_time_update( );
	if( num <= 0 )
	{
		if( num < 0 )
		{
			if( errno == EINTR )
			{
				err("ev select stop by signal\n");
				return OK;
			}
			err("ev select wait failed, [%d]\n", errno );
			return ERROR;
		}
		if( msec != -1 )
		{
			return OK;
		}
		err("ev select return 0\n");
		return ERROR;
	}

	for( i = 0; i < MAX_NET_CON; i ++ )
	{
        event_t * ev = this->ev_arr[i];
		if( ev && ev->f_active )
		{
            if( FD_ISSET( ev->fd, &this->refds ) && ev->f_read && ev->read_pt )
            {
                ev->read_pt(ev);
            }
            
            if( FD_ISSET( ev->fd, &this->wefds ) && ev->f_write && ev->write_pt )
            {
                ev->write_pt(ev);
            }
		}
	}
	return OK;
}
#endif

status event_opt( event_t * event, int32 fd, net_event_type trigger_type )
{
	if( this->g_event_handler.opt ) return this->g_event_handler.opt( event, fd, trigger_type );
	return OK;
}

status event_run( time_t msec )
{
	if( this->g_event_handler.run ) return this->g_event_handler.run( msec );
	return OK;
}

status event_alloc( event_t ** ev )
{
	queue_t * q = NULL;
	event_t * local_ev = NULL;
	if( 1 == queue_empty( &this->usable ) )
	{
		err("event usable empty\n");
		return ERROR;
	}
	q = queue_head( &this->usable );
	queue_remove( q );
	queue_insert_tail( &this->use, q );
	local_ev = l_get_struct( q, event_t, queue);
	
	*ev = local_ev;
	return OK;
}

status event_free( event_t * ev )
{
	if( ev )
	{
		if( ev->f_active && ev->type != EV_NONE ) 
		{
			event_opt( ev, ev->fd, EV_NONE );
		}
#if !defined(EVENT_EPOLL)
        // select need to free event's position info
        if( ev->f_position )
        {
            event_select_position_free( ev->idx );
            ev->f_position = 0;
        }
#endif
		ev->idx			= 0;
		ev->fd			= 0;
		
		ev->read_pt		= NULL;
		ev->write_pt	= NULL;
		ev->data		= NULL;
		
		timer_del( &ev->timer );
		ev->f_active	= 0;
		ev->f_read		= 0;
		ev->f_write		= 0;	

		queue_remove( &ev->queue );
		queue_insert_tail( &this->usable, &ev->queue );
	}
	return OK;
}

status event_init( void )
{
	uint32 i = 0;
	listen_t * listen = NULL;
	
	do 
	{	
		if( this )
		{
			err("event this not empty\n");
			return ERROR;
		}
		this = l_safe_malloc( sizeof(private_event_t) + (sizeof(event_t)*MAX_NET_CON) );
		if( !this )
		{
			err("event alloc this failed, [%d]\n", errno );
			return ERROR;
		}
		memset( this, 0, sizeof(private_event_t) + (sizeof(event_t)*MAX_NET_CON) );
        
        memset( &this->g_event_handler, 0, sizeof(event_handler_t) );
#if defined(EVENT_EPOLL)
        this->g_event_handler.init      = event_epoll_init;
        this->g_event_handler.end       = event_epoll_end;
        this->g_event_handler.opt       = event_epoll_opt;
        this->g_event_handler.run       = event_epoll_run;
#else
        this->g_event_handler.init      = event_select_init;
        this->g_event_handler.end       = event_select_end;
        this->g_event_handler.opt       = event_select_opt;
        this->g_event_handler.run       = event_select_run;
#endif
		queue_init( &this->use );
		queue_init( &this->usable );
		for( i = 0; i < MAX_NET_CON; i ++ )
		{
			queue_insert_tail( &this->usable, &this->pool[i].queue );
		}
		
		if( this->g_event_handler.init )
		{
			this->g_event_handler.init();
		}
		
		for( i = 0; i < listens->elem_num; i ++ ) 
		{
			listen = mem_list_get( listens, i+1 );
			listen->event.data 		= listen;
			listen->event.read_pt 	= l_net_accept;
			event_opt( &listen->event, listen->fd, EV_R );
		}
		return OK;
	} while(0);

	if( this )
	{
		l_safe_free( this );
	}
	return ERROR;	
}

status event_end( void )
{
	if( this->g_event_handler.end )
	{
		return this->g_event_handler.end();
	}
	if( this )
	{
		l_safe_free( this );
	}
	return OK;
}

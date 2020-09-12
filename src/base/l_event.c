#include "l_base.h"

static int32		event_fd = 0;
static struct epoll_event * events = NULL;

static void l_net_timeout( void * data )
{
	connection_t * c = data;
	net_free( c );
}

status l_net_connect( connection_t * c, struct sockaddr_in * addr, uint32 con_type )
{
	struct addrinfo * res = NULL;
	status rc;
	int fd = 0;

	if( 0 != memcmp( &c->addr, addr, sizeof(struct sockaddr_in) ) )
	{
		memcpy( &c->addr, addr, sizeof(struct sockaddr_in) );
	}

	c->con_type = con_type;
	if( c->con_type != TYPE_TCP && c->con_type != TYPE_UDP )
	{
		err("not support con type [%x]\n", con_type );
		return ERROR;
	}
	fd = socket(AF_INET, (c->con_type == TYPE_TCP) ? SOCK_STREAM : SOCK_DGRAM, 0 );
	if( ERROR == fd ) {
		err(" socket failed, [%d]\n", errno );
		return ERROR;
	}
	if( OK != l_socket_reuseaddr( fd ) ) {
		err(" reuseaddr failed\n" );
		close( fd );
		return ERROR;
	}
	if( OK != l_socket_nonblocking( fd ) ) {
		err(" nonblock failed\n" );
		close( fd );
		return ERROR;
	}
	
	rc = connect( fd, (struct sockaddr*)&c->addr, sizeof(struct sockaddr_in) );
	if( rc == ERROR ) {
		if( errno != EINPROGRESS ) {
			err(" connect failed, [%d]\n", errno );
			close( fd );
			return ERROR;
		}
		rc = AGAIN;
	}
	c->fd = fd;

	if( con_type == TYPE_TCP )
	{
		c->send 		= sends;
		c->recv 		= recvs;
		c->send_chain 	= send_chains;
		c->recv_chain 	= NULL;
	}
	return rc;	
}

status l_net_accept( event_t * ev )
{
	listen_t * listen = ev->data;
	int32 c_fd;
	connection_t * c;
	
	struct sockaddr_in client_addr;
	socklen_t len = sizeof( struct sockaddr_in );
	
	while( 1 ) 
	{
		memset( &client_addr, 0, len );
		
		c_fd = accept( listen->fd, (struct sockaddr *)&client_addr, &len );
		if( ERROR == c_fd ) 
		{
			if( errno == EWOULDBLOCK || errno == EAGAIN ) {
				return AGAIN;
			}
			err(" accept failed, [%d]\n", errno );
			return ERROR;
		}
		if( ERROR == net_alloc( &c ) ) {
			err(" client_con alloc\n" );
			close( c_fd );
			return ERROR;
		}
		memcpy( &c->addr, &client_addr, len );
		if( OK != l_socket_nonblocking( c_fd ) ) 
		{
			err(" socket nonblock failed\n" );
			net_free( c );
			return ERROR;
		}
		c->fd = c_fd;

		c->con_type 	= TYPE_TCP;
		c->recv 		= recvs;
		c->send 		= sends;
		c->recv_chain 	= NULL;
		c->send_chain 	= send_chains;

		c->ssl_flag = (listen->type == L_SSL ) ? 1 : 0;

		timer_set_data( &c->event.timer, (void*)c );
		timer_set_pt( &c->event.timer, l_net_timeout );
		timer_add( &c->event.timer, 3 );

		c->event.read_pt 	= listen->handler;
		c->event.write_pt 	= NULL;
		event_opt( &c->event, c->fd, EV_R );

		c->event.read_pt( &c->event );
	}
	return OK;
}

status event_opt( event_t * event, int32 fd, net_events events )
{
	struct epoll_event ev;
	int32	op;
	uint32 pre_flag;
	net_events pre_events, cache;
	
	cache = events;
	
	if( events == EV_R ) 
	{
		if( event->f_read == 1) 
		{
			return OK;
		}
		pre_flag 	= event->f_write;
		pre_events 	= EV_W;
	} else if ( events == EV_W ) 
	{
		if( event->f_write == 1) 
		{
			return OK;
		}
		pre_flag 	= event->f_read;
		pre_events 	= EV_R;
	} else {
		err(" not support events\n" );
		return ERROR;
	}
	
	if( pre_flag == 1) 
	{
		events |= pre_events;
		op = EPOLL_CTL_MOD;
	} 
	else 
	{
		op = EPOLL_CTL_ADD;
	}
	
	ev.data.ptr = (void*)event;
	ev.events 	= (uint32) (EPOLLET | events);
	
	if( OK != epoll_ctl( event_fd, op, fd, &ev ) ) {
		err(" epoll_ctl failed, [%d]\n", errno );
		return ERROR;
	}
	
	event->f_active = 1;
	if( cache == EV_R ) 
	{
		event->f_read = 1;
	}
	if( cache == EV_W ) 
	{
		event->f_write = 1;
	}
	return OK;
}

status event_run( time_t time_out )
{
	event_t * event;
	int32 i, action_num;
	uint32 ev;

	action_num = epoll_wait( event_fd, events, EV_MAXEVENTS, (int)time_out );
	l_time_update( );
	if( action_num == ERROR ) 
	{
		if( errno == EINTR ) 
		{
			debug(" epoll_wait interrupted by signal\n" );
			return OK;
		}
		err(" epoll_wait failed, [%d]\n", errno );
		return ERROR;
	} 
	else if( action_num == 0 ) 
	{
		if( time_out != -1 ) 
		{
			return OK;
		}
		err(" epoll_wait return 0\n" );
		return ERROR;
	}

	for( i = 0; i < action_num; i ++ ) 
	{
		event = (event_t*)events[i].data.ptr;
		if( !event->f_active ) continue;

		ev = events[i].events;
		
		if( (ev & EV_R) && event->f_active ) 
		{
			if( event->read_pt ) event->read_pt( event );
		}
		if( (ev & EV_W) && event->f_active ) 
		{
			if( event->write_pt ) event->write_pt( event );
		}
	}
	return OK;
}

status event_init( void )
{
	uint32 i;
	
	events = (struct epoll_event*) l_safe_malloc ( sizeof(struct epoll_event)*EV_MAXEVENTS );
	if( !events ) {
		err(" l_safe_malloc events\n" );
		return ERROR;
	}
	event_fd = epoll_create1(0);
	debug(" event fd [%d]\n", event_fd );
	if( event_fd == ERROR ) {
		err(" epoll create1\n" );
		return ERROR;
	}

	// init listen events
	uint32 j;
	listen_t * listen;

	for( i = 0; i < listens->elem_num; i ++ ) {
		listen = mem_list_get( listens, i+1 );

		listen->event.data 		= listen;
		listen->event.read_pt 	= l_net_accept;
	
		event_opt( &listen->event, listen->fd, EV_R );
	}
	return OK;
}

status event_end( void )
{
	if( event_fd ) {
		close( event_fd );
	}
	if( events ) {
		l_safe_free( events );
	}
	return OK;
}

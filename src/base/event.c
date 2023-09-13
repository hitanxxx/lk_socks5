#include "common.h"
#include "queue.h"

typedef struct
{
    /// evt action array
    event_t *       ev_arr[MAX_NET_CON];
    int             ev_arrn;
    /// accept evt action array
    event_t *       ev_arr_accept[128];
    int             ev_arr_acceptn;

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
	g_event_ctx->events = (struct epoll_event*) l_safe_malloc ( sizeof(struct epoll_event)*(MAX_NET_CON+128) );
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

static status event_epoll_opt( event_t * ev, int32 fd, int want_opt )
{
	struct epoll_event evsys;
	memset( &evsys, 0, sizeof(struct epoll_event) );
    
	/// want type not same as record type
	if( ev->opt != want_opt ) {
	
		if( want_opt == EV_NONE ) {
			if( ev->f_active == 1 ) {
				if( -1 == epoll_ctl( g_event_ctx->event_fd, EPOLL_CTL_DEL, fd, NULL ) ) {
					err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
					return ERROR;
				}
				ev->f_active = 0;
			} else {
				/// do nothing. event already delete
			}
		} else {
			evsys.data.ptr = (void*)ev;
            /// edge trigger
			evsys.events = EPOLLET|want_opt;
            /// level trigger
			/// evsys.events = want_opt;

			if( -1 == epoll_ctl( g_event_ctx->event_fd, ( ev->f_active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD ), fd, &evsys ) ) {
				err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
				return ERROR;
			}
			ev->f_active = 1;
		}

		if( !ev->fd ) {
            ev->fd = fd;
        }
		ev->opt = want_opt;
	}
	return OK;
}

status event_epoll_run( time_t msec )
{
	int32 i = 0, act_num = 0;

    act_num = epoll_wait( g_event_ctx->event_fd, g_event_ctx->events, MAX_NET_CON+128, (int)msec ); 
	if( act_num <= 0 ) {
		if( act_num < 0 ) {
			if( errno == EINTR ) {
				err("evt epoll_wait interrupt by signal\n");
				return OK;
			}
			err("evt epoll_wait failed, [%d] [%s]", errno, strerror(errno) );
			return ERROR;
		} else {
            /// msec -1 will not be timeout. can't return 0
            return ( (msec == -1) ? ERROR : AGAIN );
        }
	}

	for( i = 0; i < act_num; i ++ )  {
        int evopt = g_event_ctx->events[i].events;
		event_t * ev = g_event_ctx->events[i].data.ptr;
		
        if( ev->f_active ) {
            if( ev->f_listen ) {
                g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = ev;
            } else {
                if( evopt & EV_R || evopt & EV_W ) {
                    g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
                }
            }
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

static status event_select_opt( event_t * ev, int32 fd, int want_opt )
{
	if( ev->opt != want_opt ) {
    
        if( want_opt == (EV_R|EV_W) ) {
            if( !(ev->opt & EV_R) ) {
                FD_SET( fd, &g_event_ctx->rfds );
            }
            if( !(ev->opt & EV_W) ) {
                FD_SET( fd, &g_event_ctx->wfds );
            }
        } else if ( want_opt == EV_R ) {
            if( !(ev->opt & EV_R) ) {
                FD_SET( fd, &g_event_ctx->rfds );
            }
            if( ev->opt & EV_W ) {
                FD_CLR( fd, &g_event_ctx->wfds );
            }
        } else if ( want_opt == EV_W ) {
            if( !(ev->opt & EV_W) ) {
                FD_SET( fd, &g_event_ctx->wfds );
            }
            if( ev->opt & EV_R ) {
                FD_CLR( fd, &g_event_ctx->rfds );
            }
        } else {
            /// EV_NONE
            if( ev->opt & EV_W ) {
                FD_CLR( fd, &g_event_ctx->wfds );
            }
            if( ev->opt & EV_R ) {
                FD_CLR( fd, &g_event_ctx->rfds );
            }
        }

		if( want_opt == EV_NONE ) {
			ev->f_active = 0;
		} else {
			ev->f_active = 1;           
		}
        
		if( 0 == ev->fd ) {
            ev->fd = fd;
        }
		
		ev->opt = want_opt;
	}
	return OK;
}

status event_select_run( time_t msec )
{
	struct timeval tv, *p_tv = NULL;
	int i = 0, fd_max = -1;
	int actall = 0, actn = 0;
	
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
		if( (p_ev->f_active == 1) && (p_ev->fd > fd_max) ) {
			fd_max = p_ev->fd;
		}
	}
	/// find max fd in pool events 
	for( i = 0; i < MAX_NET_CON; i++ ) {
		if( (g_event_ctx->pool[i].f_active == 1) && (g_event_ctx->pool[i].fd > fd_max) ) {
			fd_max = g_event_ctx->pool[i].fd;
		}
	}

	// select return will be change read fds and write fds
	memcpy( &tmp_rfds, &g_event_ctx->rfds, sizeof(fd_set) );
	memcpy( &tmp_wfds, &g_event_ctx->wfds, sizeof(fd_set) );

	
	actall = select( fd_max + 1, &tmp_rfds, &tmp_wfds, NULL, p_tv );
	if( actall <= 0 ) {
		if( actall < 0 ) {
			if( errno == EINTR ) {
				err("evt select interrupt by signal\n");
				return OK;
			}
			err("evt select failed, [%d] [%s]\n", errno, strerror(errno) );
			return ERROR;
		} else {
            /// msec -1 will not be timeout. can't return 0
            return ( (msec == -1) ? ERROR : AGAIN );
        }
	}

	
    /// loop check all listen events
	for( i = 0; (i < listens->elem_num) && (actn < actall); i ++ ) {
		listen_t * p_listen = mem_arr_get( listens, i + 1 );
		event_t * p_ev = &p_listen->event;
		if( p_ev->f_active == 1 ) {
			if( FD_ISSET( p_ev->fd, &tmp_rfds ) ) {
				g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = p_ev;
	            actn ++;   
			}         
		}
	}
	
	/// loop check all events 
	for( i = 0; (i < MAX_NET_CON) && (actn < actall); i ++ ) {

        event_t * p_ev = &g_event_ctx->pool[i];
        if( p_ev->f_active == 1 ) {
            if( FD_ISSET( p_ev->fd, &tmp_rfds ) || FD_ISSET( p_ev->fd, &tmp_wfds ) ) {
                g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = p_ev;
                actn ++;
            }
        }
    } 
	
	return OK;
}
#endif

status event_post_event(  event_t * ev )
{	
	g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
	return OK;
}

status event_opt( event_t * event, int32 fd, int want_opt )
{
	return g_event_ctx->g_event_handler.opt( event, fd, want_opt );
}

status event_run( time_t msec )
{	
	int i = 0;
	listen_t * listen = NULL;

    /// listen fd use SO_REUSEPORT. don't need do mutex again 
    /// kernel will do it 
    if( 0 ) {
        /// if process current used event num < all*80%, then try to listen
    	if( g_event_ctx->queue_use_num <= (MAX_NET_CON*80/100) ) {
    		// try to get listen event lock
    		process_lock();
    		if( process_mutex_value_get() == 0 ) {
    			for( i = 0; i < listens->elem_num; i ++ ) {
    				listen = mem_arr_get( listens, i+1 );
    				listen->event.data = listen;
    				listen->event.read_pt = net_accept;
    				event_opt( &listen->event, listen->fd, EV_R );
    			}
    			process_mutex_value_set( proc_pid() );
    		}
    		process_unlock();
    	}
    }

    /// clear evt action array
    g_event_ctx->ev_arrn = 0;
    g_event_ctx->ev_arr_acceptn = 0;
    /// event run loop
	g_event_ctx->g_event_handler.run( msec );
	/// systime update 
	systime_update( );

    for( i = 0; i < g_event_ctx->ev_arr_acceptn; i ++ ) {
        event_t * p_ev = g_event_ctx->ev_arr_accept[i];
        
        if( p_ev->opt & EV_R ) {
            if( p_ev->read_pt ) p_ev->read_pt(p_ev);
        }
    }

    for( i = 0; i < g_event_ctx->ev_arrn; i ++ ) {
        event_t * p_ev = g_event_ctx->ev_arr[i];

        if( p_ev->opt & EV_R ) {
            if( p_ev->read_pt ) p_ev->read_pt(p_ev);
        }
        if( p_ev->opt & EV_W ) {
            if( p_ev->write_pt ) p_ev->write_pt(p_ev);
        }
    }

    /// listen fd use SO_REUSEPORT. don't need do mutex again 
	if( 0 ) {
	    process_lock();
    	if( process_mutex_value_get() == proc_pid() ) {
    		for( i = 0; i < listens->elem_num; i ++ ) {
    			listen = mem_arr_get( listens, i+1 );
    			listen->event.data = listen;
    			listen->event.read_pt = net_accept;
    			event_opt( &listen->event, listen->fd, EV_NONE );
    		}	
    		process_mutex_value_set(0);
    	}
    	process_unlock();
	}
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
		if( ev->f_active && ev->opt != EV_NONE )  {
			event_opt( ev, ev->fd, EV_NONE );
		}
		ev->opt = 0;
		
		ev->fd = 0;
		ev->read_pt = NULL;
		ev->write_pt = NULL;
		ev->data = NULL;
		
		timer_del( &ev->timer );
		ev->f_active = 0;
		
		queue_remove( &ev->queue );
		queue_insert_tail( &g_event_ctx->usable, &ev->queue );
		g_event_ctx->queue_use_num --;
	}
	return OK;
}

status event_init( void )
{	
    int i = 0;
	if( g_event_ctx ) {
		err("g_event_ctx not empty\n");
		return ERROR;
	}
	g_event_ctx = l_safe_malloc( sizeof(g_event_t) + (sizeof(event_t)*MAX_NET_CON) );
	if( !g_event_ctx ) {
		err("event alloc this failed, [%d]\n", errno );
		return ERROR;
	}
	
	queue_init( &g_event_ctx->use );
	queue_init( &g_event_ctx->usable );
	for( i = 0; i < MAX_NET_CON; i ++ ) {
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

    if(1) {
        /// all worker process will be add listen fd into event.
        /// listen fd set by SO_REUSEPORT. 
        /// kernel will be process listen fd loadbalance
        int i = 0;
        for( i = 0; i < listens->elem_num; i ++ )  {
    		listen_t * listen_obj = mem_arr_get( listens, i+1 );
    		listen_obj->event.data = listen_obj;
    		listen_obj->event.read_pt = net_accept;
    		listen_obj->event.f_listen = 1;
    		event_opt( &listen_obj->event, listen_obj->fd, EV_R );
    	}
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
    		for( i = 0; i < listens->elem_num; i ++ ) {
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

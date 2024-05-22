#include "common.h"
#include "queue.h"

typedef struct
{
    /// accept evt action array
    event_t * ev_arr_accept[128];
    short ev_arr_acceptn;

    /// evt action array
    event_t * ev_arr[MAX_NET_CON];
    short ev_arrn;

    /// event action array for storge evt set by action array object
    event_t * ev_arr_back[MAX_NET_CON];
    short ev_arr_backn;

#if defined(EVENT_EPOLL)
    int           event_fd;
    struct epoll_event * events;
#else
    fd_set          rfds;
    fd_set          wfds;
    queue_t  evqueue;
#endif
    event_handler_t g_event_handler;    
    short queue_use_num;
} g_event_t;
static g_event_t * g_event_ctx = NULL;


#if defined(EVENT_EPOLL)
static status event_epoll_init(     )
{
    g_event_ctx->events = (struct epoll_event*) mem_pool_alloc ( sizeof(struct epoll_event)*(MAX_NET_CON+128) );
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
        mem_pool_free( g_event_ctx->events );
    }
    return OK;
}

static status event_epoll_opt( event_t * ev, int32 fd, int want_opt )
{
    /// want type not same as record type
    if( ev->opt != want_opt ) {
    
        if( want_opt == EV_NONE ) {
            if( -1 == epoll_ctl( g_event_ctx->event_fd, EPOLL_CTL_DEL, fd, NULL ) ) {
                err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
                return ERROR;
            }
            ev->f_active = 0;
        } else {
            struct epoll_event evsys;
            memset( &evsys, 0, sizeof(struct epoll_event) );
        
            evsys.data.ptr = (void*)ev;
            evsys.events = EPOLLET|want_opt;
            
            if( -1 == epoll_ctl( g_event_ctx->event_fd, ( ev->f_active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD ), fd, &evsys ) ) {
                err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
                return ERROR;
            }
            ev->f_active = 1;
        }

        if( !ev->fd ) ev->fd = fd;
        
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
                err("evt epoll_wait irq by [sig]\n");
                return OK;
            }
            err("evt epoll_wait irq by [err], [%d] [%s]", errno, strerror(errno) );
            return ERROR;
        } else {
            ///msec -1 will not be timeout. can't return 0
            return ((msec==-1)?ERROR:AGAIN);
        }
    }

    for( i = 0; i < act_num; i ++ ) {
        /// events contains this fd action bit field
        event_t * act_ev = g_event_ctx->events[i].data.ptr;
        int act_opt = g_event_ctx->events[i].events;
        if( act_ev->f_active ) {
            if( act_ev->f_listen ) {
                /// listen must be readable
                if( act_opt & EV_R ) {
                    g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = act_ev;
                    act_ev->f_read = 1;
                }
            } else {
                /// mark action type of event. (EV_R/EV_W/(EV_R & EV_W))
                if( (act_opt & EV_R) || (act_opt & EV_W) ) {
                    g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = act_ev;
                    if( act_opt & EV_R ) act_ev->f_read = 1;
                    if( act_opt & EV_W ) act_ev->f_write = 1;
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

        ev->f_active = ( want_opt == EV_NONE ) ? 0 : 1;
        if( 0 == ev->fd ) ev->fd = fd;
        
        ev->opt = want_opt;
    }
    return OK;
}

status event_select_run( time_t msec )
{
    struct timeval wait_tm;
    int fdmax = -1;
    int actall = 0, actn = 0;
    
    fd_set rfds;
    fd_set wfds;

    memset( &wait_tm, 0, sizeof(struct timeval) );
    if( msec > 0 ) {
        wait_tm.tv_sec = msec/1000;
        wait_tm.tv_usec = (msec%1000)*1000;
    } else {
        wait_tm.tv_sec = 0;
        wait_tm.tv_usec = (200*1000);
    }
    
    /// find max fd in listen events
    listen_t * p = g_listens;
    while(p) {
        event_t * pev = &p->event;
        if(pev->f_active && pev->fd > fdmax) {
            fdmax = pev->fd;
        }
        p = p->next;
    }
    /// find max fd in event queue
    queue_t * q = queue_head(&g_event_ctx->evqueue);
    queue_t * n = NULL;
    while( q != queue_tail( &g_event_ctx->evqueue )) {
        n = queue_next(q);
        event_t * pev = ptr_get_struct( q, event_t, queue );
        if( pev->f_active && pev->fd > fdmax ) {
            fdmax = pev->fd;
        }
        q = n;
    }

    /// select return will be change read fds and write fds
    memcpy( &rfds, &g_event_ctx->rfds, sizeof(fd_set) );
    memcpy( &wfds, &g_event_ctx->wfds, sizeof(fd_set) );

    
    actall = select( fdmax + 1, &rfds, &wfds, NULL, &wait_tm );
    if( actall <= 0 ) {
        if( actall < 0 ) {
            if( errno == EINTR ) {
                err("evt select irq by [sig]\n");
                return OK;
            }
            err("evt select irq by [err], [%d] [%s]\n", errno, strerror(errno) );
            return ERROR;
        } else {
            /// msec -1 will not be timeout. can't return 0
            return ( (msec == -1) ? ERROR : AGAIN );
        }
    }
    
    /// loop check all listen events
    p = g_listens;
    while(p && actn < actall) {
        event_t * pev = &p->event;
        if(pev->f_active) {
            if( FD_ISSET( pev->fd, &rfds ) ) {
                g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = pev;
                pev->f_read = 1;
                actn ++; 
            }
        }
        p = p->next;
    }
    /// loop check all events 
    q = queue_head(&g_event_ctx->evqueue);
    n = NULL;
    while( q != queue_tail( &g_event_ctx->evqueue ) && actn < actall ) {
        n = queue_next(q);
        event_t * pev = ptr_get_struct( q, event_t, queue );
        if( pev->f_active) {
            if( FD_ISSET( pev->fd, &rfds ) || FD_ISSET( pev->fd, &wfds ) ) {
                g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = pev;
                if( FD_ISSET( pev->fd, &rfds ) ) pev->f_read = 1;
                if( FD_ISSET( pev->fd, &wfds ) ) pev->f_write = 1;
                actn ++;
            }
        }
        q = n;
    }
    
    return OK;
}
#endif

status event_post_event(  event_t * ev )
{    
    g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
    return OK;
}

status event_post_backevent( event_t * ev ) 
{
    g_event_ctx->ev_arr_back[g_event_ctx->ev_arr_backn++] = ev;
    return OK;
}

status event_opt( event_t * event, int32 fd, int want_opt )
{
    return g_event_ctx->g_event_handler.opt( event, fd, want_opt );
}

status event_run( time_t msec )
{    
    int i = 0;

    /// listen fd use SO_REUSEPORT. don't need do mutex (kernel will do it)
    if( 0 ) {
        /// if process current used event num < all*80%, then try to listen
        if( g_event_ctx->queue_use_num <= (MAX_NET_CON*80/100) ) {
            /// try to get listen event lock
            process_lock();
            if( process_mutex_value_get() == 0 ) {
                listen_t * p = g_listens;
                while(p) {
                    p->event.data = p;
                    p->event.read_pt = net_accept;
                    p->event.f_listen = 1;
                    event_opt( &p->event, p->fd, EV_R );
                    p = p->next;
                }
                process_mutex_value_set( proc_pid() );
            }
            process_unlock();
        }
    }

    /// clear action array
    g_event_ctx->ev_arrn = 0;
    g_event_ctx->ev_arr_acceptn = 0;
    g_event_ctx->ev_arr_backn = 0;
    /// event run loop
    g_event_ctx->g_event_handler.run( msec );
    /// systime update 
    systime_update( );

    /// proc ev arr accept 
    for( i = 0; i < g_event_ctx->ev_arr_acceptn; i ++ ) {
        event_t * p_ev = g_event_ctx->ev_arr_accept[i];
        if( p_ev->opt & EV_R ) {
            /// listen event don't only check read 
            if( p_ev->f_read ) {
                if( p_ev->read_pt ) p_ev->read_pt(p_ev);
                p_ev->f_read= 0;
            }
        }
    }
    /// proc ev arr
    for( i = 0; i < g_event_ctx->ev_arrn; i ++ ) {
        event_t * p_ev = g_event_ctx->ev_arr[i];
        
        if( p_ev->opt & EV_R ) {
            /// if f_read has been actived. means fd need read this time
            if( p_ev->f_read ) {
                if( p_ev->read_pt ) p_ev->read_pt(p_ev);
                p_ev->f_read = 0;
            }
        }
        if( p_ev->opt & EV_W ) {
            /// if f_write has been actived. means fd need write this time
            if( p_ev->f_write ) {
                if( p_ev->write_pt ) p_ev->write_pt(p_ev);
                p_ev->f_write = 0;
            }
        }
    }
    /// proc ev arr back
    for( i = 0; i < g_event_ctx->ev_arr_backn; i ++ ) {
        event_t * p_ev = g_event_ctx->ev_arr_back[i];
        
        if( p_ev->opt & EV_R ) {
            /// if f_read has been actived. means fd need read this time
            if( p_ev->f_read ) {
                if( p_ev->read_pt ) p_ev->read_pt(p_ev);
                p_ev->f_read = 0;
            }
        }
        if( p_ev->opt & EV_W ) {
            /// if f_write has been actived. means fd need write this time
            if( p_ev->f_write ) {
                if( p_ev->write_pt ) p_ev->write_pt(p_ev);
                p_ev->f_write = 0;
            }
        }
    }
    

    /// listen fd use SO_REUSEPORT. don't need do mutex 
    if( 0 ) {
        process_lock();
        if( process_mutex_value_get() == proc_pid() ) {
            listen_t * p = g_listens;
            while(p) {
                p->event.data = p;
                p->event.read_pt = net_accept;
                p->event.f_listen = 1;
                event_opt( &p->event, p->fd, EV_NONE );
                p = p->next;
            }
            process_mutex_value_set(0);
        }
        process_unlock();
    }
    return 0;
}

status event_alloc( event_t ** ev )
{
    event_t * nev = mem_pool_alloc( sizeof(event_t) );
    if(!nev){
        err("evt alloc nev failed\n");
        return ERROR;
    }
    g_event_ctx->queue_use_num ++;
#ifndef EVENT_EPOLL
    queue_insert_tail( &g_event_ctx->evqueue, &nev->queue );
#endif
    *ev = nev;
    return OK;
}

status event_free( event_t * ev )
{
    if( ev ) {
        if( ev->f_active && ev->opt != EV_NONE )  {
            event_opt( ev, ev->fd, EV_NONE );
        }
        
        ev->fd = 0;
        timer_del( &ev->timer );
        ev->data = NULL;
        ev->opt = 0;
        
        ev->f_active = 0;
        ev->f_read = ev->f_write = 0;
        ev->read_pt = ev->write_pt = NULL;

        g_event_ctx->queue_use_num --;
#ifndef EVENT_EPOLL
        queue_remove( &ev->queue );
#endif
        mem_pool_free(ev);
    }
    return OK;
}

status event_init( void )
{    
    if( g_event_ctx ) {
        err("g_event_ctx not empty\n");
        return ERROR;
    }
    g_event_ctx = mem_pool_alloc( sizeof(g_event_t) );
    if( !g_event_ctx ) {
        err("event alloc this failed\n" );
        return ERROR;
    }

#if defined(EVENT_EPOLL)
    g_event_ctx->g_event_handler.init = event_epoll_init;
    g_event_ctx->g_event_handler.end = event_epoll_end;
    g_event_ctx->g_event_handler.opt = event_epoll_opt;
    g_event_ctx->g_event_handler.run = event_epoll_run;
#else
    g_event_ctx->g_event_handler.init = event_select_init;
    g_event_ctx->g_event_handler.end = event_select_end;
    g_event_ctx->g_event_handler.opt = event_select_opt;
    g_event_ctx->g_event_handler.run = event_select_run;
    queue_init(&g_event_ctx->evqueue);
#endif
    // init event, and add listen into event
    g_event_ctx->g_event_handler.init();

    if(1) {
        /// all worker process will be add listen fd into event.
        /// listen fd set by SO_REUSEPORT. 
        /// kernel will be process listen fd loadbalance
        listen_t * p = g_listens;
        while(p) {
            p->event.data = p;
            p->event.read_pt = net_accept;
            p->event.f_listen = 1;
            event_opt( &p->event, p->fd, EV_R );
            p = p->next;
        }
    }
    return OK;
    
}

status event_end( void )
{
    if( g_event_ctx ) {
        process_lock();
        if( process_mutex_value_get() == proc_pid() ) {
            listen_t * p = g_listens;
            while(p) {
                p->event.data = p;
                p->event.read_pt = net_accept;
                p->event.f_listen = 1;
                event_opt( &p->event, p->fd, EV_NONE );
                p = p->next;
            }
            process_mutex_value_set(0);
        }
        process_unlock();
        
        if( g_event_ctx->g_event_handler.end ) {
            g_event_ctx->g_event_handler.end();
        }
        mem_pool_free( g_event_ctx );
        g_event_ctx = NULL;
    }

    return OK;
}

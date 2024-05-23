#include "common.h"
#include "queue.h"

typedef struct
{
    /// accept evt action array
    event_t * ev_arr_accept[128];
    short ev_arr_acceptn;

    /// evt action array
    event_t * ev_arr[MAX_NET_CON*2];
    short ev_arrn;
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
        if(want_opt==EV_NONE) {
            if( -1 == epoll_ctl( g_event_ctx->event_fd, EPOLL_CTL_DEL, fd, NULL ) ) {
                err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
                return ERROR;
            }
        } else {
            struct epoll_event evsys;
            memset( &evsys, 0, sizeof(struct epoll_event) );
            evsys.data.ptr = (void*)ev;
            evsys.events = EPOLLET|want_opt;
            if(-1 == epoll_ctl(g_event_ctx->event_fd, (ev->opt!=EV_NONE?EPOLL_CTL_MOD:EPOLL_CTL_ADD), fd, &evsys) ) {
                err("evt epoll_ctl fd [%d] error. want_opt [%x], errno [%d] [%s]\n", fd, want_opt, errno, strerror(errno) );
                return ERROR;
            }
        }
        if(!ev->fd) ev->fd=fd;
        ev->opt=want_opt;
    }
    return OK;
}

status event_epoll_run( time_t msec )
{
    int32 i = 0, all = 0;

    all = epoll_wait( g_event_ctx->event_fd, g_event_ctx->events, MAX_NET_CON+128, (int)msec ); 
    if(all<=0) {
        if(all==0) {
            return (msec==-1)?ERROR:AGAIN;
        }
        if(errno==EINTR) {
            err("evt epoll_wait irq by [sig]\n");
            return OK;
        }
        err("evt epoll_wait irq by [err], [%d] [%s]", errno, strerror(errno) );
        return ERROR;
    }

    for(i=0;i<all;i++) {
        event_t * ev = g_event_ctx->events[i].data.ptr;
        if(ev->flisten) {
            ev->fread=1;
            g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = ev;
        } else {
            int opt = g_event_ctx->events[i].events;
            if(opt&EV_R) {
                ev->fread=1;
                ev->idxr=g_event_ctx->ev_arrn;
                g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
            }
            if(opt&EV_W) {
                ev->fwrite=1;
                ev->idxw=g_event_ctx->ev_arrn;
                g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
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
    event_t * ev = NULL;
    queue_t * q = NULL;
  
    fd_set rfds;
    fd_set wfds;

    memset( &wait_tm, 0, sizeof(struct timeval) );
    if(msec>0) {
        wait_tm.tv_sec = msec/1000;
        wait_tm.tv_usec = (msec%1000)*1000;
    }
    
    /// find max fd in listen events
    listen_t * p = g_listens;
    while(p) {
        ev = &p->event;
        if(ev->fd>fdmax) {
            fdmax=ev->fd;
        }
        p = p->next;
    }
    /// find max fd in event queue
    q = queue_head(&g_event_ctx->evqueue);
    while( q != queue_tail( &g_event_ctx->evqueue )) {
        ev = ptr_get_struct( q, event_t, queue );
        if(ev->fd>fdmax) {
            fdmax = ev->fd;
        }
        q = queue_next(q);
    }

    /// select return will be change read fds and write fds
    memcpy( &rfds, &g_event_ctx->rfds, sizeof(fd_set) );
    memcpy( &wfds, &g_event_ctx->wfds, sizeof(fd_set) );
    actall = select( fdmax + 1, &rfds, &wfds, NULL, msec!=-1?&wait_tm:NULL );
    if(actall<= 0) {
        if(actall==0) {
            return msec==-1?ERROR:AGAIN;
        }
        if( errno == EINTR ) {
            err("evt select irq by [sig]\n");
            return OK;
        }
        err("evt select irq by [err], [%d] [%s]\n", errno, strerror(errno) );
        return ERROR;
    }
    
    p = g_listens;
    while(p && actn < actall) {
        ev = &p->event;
        if( FD_ISSET(ev->fd, &rfds) ) {
            ev->fread = 1;
            g_event_ctx->ev_arr_accept[g_event_ctx->ev_arr_acceptn++] = ev;
            actn ++;
        }
        p = p->next;
    }

    q = queue_head(&g_event_ctx->evqueue);
    while( q != queue_tail( &g_event_ctx->evqueue ) && actn < actall ) {
        ev = ptr_get_struct( q, event_t, queue );
        if(FD_ISSET(ev->fd, &rfds)) {
            ev->fread=1;
            ev->idxr=g_event_ctx->ev_arrn;
            g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
        }
        if(FD_ISSET(ev->fd, &wfds)) {
            ev->fwrite=1;
            ev->idxw=g_event_ctx->ev_arrn;
            g_event_ctx->ev_arr[g_event_ctx->ev_arrn++] = ev;
        }
        q = queue_next(q);
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
    event_t *ev = NULL;
    g_event_ctx->ev_arr_acceptn = 0;
    g_event_ctx->ev_arrn = 0;
    g_event_ctx->g_event_handler.run( msec );
    systime_update( );
    
    for(i=0;i<g_event_ctx->ev_arr_acceptn;i++) {
        ev = g_event_ctx->ev_arr_accept[i];
        if(ev&&ev->fread) {
            if(ev->read_pt) ev->read_pt(ev);
            ev->fread= 0;
        }
    }
    for(i=0;i<g_event_ctx->ev_arrn;i++) {
        ev = g_event_ctx->ev_arr[i];
        if(ev) {
            if(ev->fread) {
                ev->fread=0;
                if(ev->read_pt) ev->read_pt(ev);
            } else if (ev->fwrite) {
                ev->fwrite=0;
                if(ev->write_pt) ev->write_pt(ev);
            }
        }
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
        timer_del( &ev->timer );
        if(!ev->flisten) {
            if(ev->idxr) g_event_ctx->ev_arr[ev->idxr] = NULL;
            if(ev->idxw) g_event_ctx->ev_arr[ev->idxw] = NULL;
        }
        if(ev->opt!=EV_NONE) event_opt(ev, ev->fd, EV_NONE);       
        g_event_ctx->queue_use_num--;
#ifndef EVENT_EPOLL
        queue_remove(&ev->queue);
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

    /// all worker process will be add listen fd into event.
    /// listen fd set by SO_REUSEPORT. 
    /// kernel will be process listen fd loadbalance
    listen_t * p = g_listens;
    while(p) {
        p->event.data = p;
        p->event.read_pt = net_accept;
        p->event.flisten = 1;
        event_opt( &p->event, p->fd, EV_R );
        p = p->next;
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
                p->event.flisten = 1;
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

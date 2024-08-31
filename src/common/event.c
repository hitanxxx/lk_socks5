#include "common.h"
#include "queue.h"

typedef struct {
    event_t * evs_accept[128];
    int evs_acceptn;
    event_t * evs[MAX_NET_CON * 2]; 
    int evsn;
    
#if defined(EVENT_EPOLL)
    int epfd;
    struct epoll_event * epev;
#else
    fd_set  rfds;
    fd_set  wfds;
    queue_t  evqueue;
#endif
} g_event_t;

static g_event_t * g_event_ctx = NULL;
static event_handler_t g_event_handler = {NULL, NULL, NULL, NULL};

#if defined(EVENT_EPOLL)
static int event_epoll_init(     )
{
    g_event_ctx->epev = (struct epoll_event*) mem_pool_alloc(sizeof(struct epoll_event) * (MAX_NET_CON + 128));
    schk(g_event_ctx->epev, return -1);
    
    g_event_ctx->epfd = epoll_create1(0);
    schk(g_event_ctx->epfd, return -1);
    return 0;
}
static int event_epoll_end()
{
    if(g_event_ctx->epfd)
        close(g_event_ctx->epfd);
    
    if(g_event_ctx->epev)
        mem_pool_free( g_event_ctx->epev);

    return 0;
}

static int event_epoll_opt(event_t * ev, int fd, int want_opt)
{
    if(ev->opt != want_opt) { ///want type not same as record type
        if(want_opt == EV_NONE) {
            schk(epoll_ctl(g_event_ctx->epfd, EPOLL_CTL_DEL, fd, NULL) != -1, return -1);
        } else {
            struct epoll_event evsys;
            memset(&evsys, 0, sizeof(struct epoll_event));
            evsys.data.ptr = (void*)ev;
            evsys.events = EPOLLET|want_opt;
            schk(epoll_ctl(g_event_ctx->epfd, (ev->opt != EV_NONE ? EPOLL_CTL_MOD : EPOLL_CTL_ADD), fd, &evsys) != -1, return -1);
        }
        if(!ev->fd) ev->fd = fd;
        ev->opt = want_opt;
    }
    return 0;
}

int event_epoll_run(time_t msec)
{
    int i = 0;
    int all = epoll_wait(g_event_ctx->epfd, g_event_ctx->epev, MAX_NET_CON + 128, (int)msec); 
    if(all <= 0) {
        if(all == 0) {
            return (msec == -1) ? -1 : -11;
        }
        if(errno == EINTR) {
            err("evt epoll_wait irq by [syscall]\n");
            return 0;
        }
        err("evt epoll_wait irq by [err], [%d] [%s]", errno, strerror(errno));
        return -1;
    }
    for(i=0; i < all; i++) {
        event_t * ev = g_event_ctx->epev[i].data.ptr;
        if(ev->flisten) {
            ev->fread = 1;
            g_event_ctx->evs_accept[g_event_ctx->evs_acceptn++] = ev;
        } else {
            int opt = g_event_ctx->epev[i].events;
            if(opt & EV_R) {
                ev->fread = 1;
                ev->idxr = g_event_ctx->evsn;
                g_event_ctx->evs[g_event_ctx->evsn++] = ev;
            }
            if(opt & EV_W) {
                ev->fwrite = 1;
                ev->idxw = g_event_ctx->evsn;
                g_event_ctx->evs[g_event_ctx->evsn++] = ev;
            }
        }
    }
    return 0;
}
#else
static int event_select_init(void)
{
    FD_ZERO(&g_event_ctx->rfds);
    FD_ZERO(&g_event_ctx->wfds);
    return 0;
}

static int event_select_end(void)
{
    FD_ZERO(&g_event_ctx->rfds);
    FD_ZERO(&g_event_ctx->wfds);
    return 0;
}

static int event_select_opt(event_t * ev, int fd, int want_opt)
{
    if(ev->opt != want_opt) {
        if(want_opt == (EV_R|EV_W)) {
            if(!(ev->opt & EV_R)) {
                FD_SET(fd, &g_event_ctx->rfds);
            }
            if(!(ev->opt & EV_W)) {
                FD_SET(fd, &g_event_ctx->wfds);
            }
        } else if (want_opt == EV_R) {
            if(!(ev->opt & EV_R)) {
                FD_SET(fd, &g_event_ctx->rfds);
            }
            if(ev->opt & EV_W) {
                FD_CLR(fd, &g_event_ctx->wfds);
            }
        } else if (want_opt == EV_W) {
            if(!(ev->opt & EV_W)) {
                FD_SET(fd, &g_event_ctx->wfds);
            }
            if(ev->opt & EV_R) {
                FD_CLR(fd, &g_event_ctx->rfds);
            }
        } else { ///EV_NONE
            if( ev->opt & EV_W ) {
                FD_CLR( fd, &g_event_ctx->wfds );
            }
            if( ev->opt & EV_R ) {
                FD_CLR( fd, &g_event_ctx->rfds );
            }
        }
        if(0 == ev->fd) ev->fd = fd;
        ev->opt = want_opt;
    }
    return 0;
}

int event_select_run(time_t msec)
{
    struct timeval wait_tm;
    int fdmax = -1;
    int actall = 0, actn = 0;
    event_t * ev = NULL;
    queue_t * q = NULL;
  
    fd_set rfds;
    fd_set wfds;
    memset(&wait_tm, 0, sizeof(struct timeval));
    if(msec > 0) {
        wait_tm.tv_sec = msec/1000;
        wait_tm.tv_usec = (msec%1000)*1000;
    }
    
    listen_t * p = g_listens; ///find max fd in listen events
    while(p) {
        ev = &p->event;
        if(ev->fd > fdmax) {
            fdmax = ev->fd;
        }
        p = p->next;
    }
    
    q = queue_head(&g_event_ctx->evqueue); ///find max fd in event queue
    while(q != queue_tail(&g_event_ctx->evqueue)) {
        ev = ptr_get_struct(q, event_t, queue);
        if(ev->fd > fdmax) {
            fdmax = ev->fd;
        }
        q = queue_next(q);
    }

    memcpy(&rfds, &g_event_ctx->rfds, sizeof(fd_set)); ///select return will be change read fds and write fds
    memcpy(&wfds, &g_event_ctx->wfds, sizeof(fd_set));
    actall = select(fdmax + 1, &rfds, &wfds, NULL, (msec != -1) ? &wait_tm : NULL);
    if(actall <= 0) {
        if(actall == 0) {
            return (msec == -1) ? -1 : -11;
        }
        if(errno == EINTR) {
            err("evt select irq by [syscall]\n");
            return 0;
        }
        err("evt select irq by [err], [%d] [%s]\n", errno, strerror(errno));
        return -1;
    }
    
    p = g_listens;
    while(p && actn < actall) {
        ev = &p->event;
        if(FD_ISSET(ev->fd, &rfds)) {
            ev->fread = 1;
            g_event_ctx->evs_accept[g_event_ctx->evs_acceptn++] = ev;
            actn ++;
        }
        p = p->next;
    }

    q = queue_head(&g_event_ctx->evqueue);
    while(q != queue_tail(&g_event_ctx->evqueue) && actn < actall) {
        ev = ptr_get_struct(q, event_t, queue);
        if(FD_ISSET(ev->fd, &rfds)) {
            ev->fread = 1;
            ev->idxr = g_event_ctx->evsn;
            g_event_ctx->evs[g_event_ctx->evsn++] = ev;
        }
        if(FD_ISSET(ev->fd, &wfds)) {
            ev->fwrite = 1;
            ev->idxw = g_event_ctx->evsn;
            g_event_ctx->evs[g_event_ctx->evsn++] = ev;
        }
        q = queue_next(q);
    }
    return 0;
}
#endif

int event_post_event(  event_t * ev)
{    
    g_event_ctx->evs[g_event_ctx->evsn++] = ev;
    return 0;
}

int event_opt(event_t * event, int fd, int want_opt)
{
    return (g_event_handler.opt ? g_event_handler.opt(event, fd, want_opt) : 0);
}

int event_run(time_t msec)
{    
    int i = 0;
    event_t *ev = NULL;
    g_event_ctx->evs_acceptn = 0;
    g_event_ctx->evsn = 0;
    
    if(g_event_handler.run) g_event_handler.run(msec);
    systime_update();
    
    for(i = 0; i < g_event_ctx->evs_acceptn; i++) {
        ev = g_event_ctx->evs_accept[i];
        if(ev && ev->fread) {
            if(ev->read_pt) ev->read_pt(ev);
            ev->fread= 0;
        }
    }
    for(i = 0; i<g_event_ctx->evsn; i++) {
        ev = g_event_ctx->evs[i];
        if(ev) {
            if(ev->fread) {
                ev->fread = 0;
                if(ev->read_pt) ev->read_pt(ev);
            } else if (ev->fwrite) {
                ev->fwrite = 0;
                if(ev->write_pt) ev->write_pt(ev);
            }
        }
    }
    return 0;
}

int event_alloc(event_t ** ev)
{
    event_t * nev = mem_pool_alloc(sizeof(event_t));
    schk(nev, return -1);
#ifndef EVENT_EPOLL
    queue_insert_tail(&g_event_ctx->evqueue, &nev->queue);
#endif
    *ev = nev;
    return 0;
}

int event_free(event_t * ev)
{
    if(ev) {
        timer_del(&ev->timer);
        if(!ev->flisten) {
            if(ev->idxr) g_event_ctx->evs[ev->idxr] = NULL;
            if(ev->idxw) g_event_ctx->evs[ev->idxw] = NULL;
        }
        if(ev->opt != EV_NONE) event_opt(ev, ev->fd, EV_NONE);
#ifndef EVENT_EPOLL
        queue_remove(&ev->queue);
#endif
        mem_pool_free(ev);
    }
    return 0;
}

int event_init(void)
{    
    schk(!g_event_ctx, return -1);
    g_event_ctx = mem_pool_alloc(sizeof(g_event_t));
    schk(g_event_ctx, return -1);

    ///def select driver
#if defined(EVENT_EPOLL)
    g_event_handler.init = event_epoll_init;
    g_event_handler.end = event_epoll_end;
    g_event_handler.opt = event_epoll_opt;
    g_event_handler.run = event_epoll_run;
#else
    g_event_handler.init = event_select_init;
    g_event_handler.end = event_select_end;
    g_event_handler.opt = event_select_opt;
    g_event_handler.run = event_select_run;
#endif    
    if(g_event_handler.init)
        g_event_handler.init();

    /// all worker process will be add listen fd into event.
    /// listen fd set by SO_REUSEPORT. 
    /// kernel will be process listen fd loadbalance
    listen_t * p = g_listens;
    while(p) {
        p->event.flisten = 1;
        p->event.read_pt = net_accept;
        p->event.data = p;
        event_opt(&p->event, p->fd, EV_R);
        p = p->next;
    }
    return 0;
}

int event_end(void)
{
    if(g_event_ctx) {
        listen_t * p = g_listens;
        while(p) {
            p->event.flisten = 1;
            p->event.read_pt = net_accept;
            p->event.data = p;
            event_opt(&p->event, p->fd, EV_NONE);
            p = p->next;
        }
        
        if(g_event_handler.end)
            g_event_handler.end();
        
        mem_pool_free(g_event_ctx);
        g_event_ctx = NULL;
    }
    return 0;
}

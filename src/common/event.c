#include "common.h"
#include "queue.h"

typedef struct {
    queue_t evqueue;
    int evn;
    ev_t *evs[MAX_NET_CON * 2];
    
#if defined(EVENT_EPOLL)
    int epfd;
    struct epoll_event *epev;
#else
    fd_set rfds;
    fd_set wfds;
#endif
} g_event_t;

static g_event_t *g_event_ctx = NULL;
static event_handler_t g_event_handler = {NULL, NULL, NULL, NULL};

#if defined(EVENT_EPOLL)
static int ev_epoll_init() {
    g_event_ctx->epev = mem_pool_alloc(sizeof(struct epoll_event) * MAX_NET_CON);
    schk(g_event_ctx->epev, return -1);
    
    g_event_ctx->epfd = epoll_create1(0);
    schk(g_event_ctx->epfd >= 0, return -1);
    return 0;
}

static int ev_epoll_end() {
    if (g_event_ctx->epfd) 
        close(g_event_ctx->epfd);    

    if (g_event_ctx->epev) 
        mem_pool_free( g_event_ctx->epev);
    return 0;
}

static int ev_epoll_opt(con_t *c, int want_opt) {
    /// want type not same as record type
    if (c->ev->opt != want_opt) { 
        if (want_opt == EV_NONE) {
            schk(epoll_ctl(g_event_ctx->epfd, EPOLL_CTL_DEL, c->fd, NULL) != -1, return -1);
        } else {
            struct epoll_event evsys;
            memset(&evsys, 0, sizeof(struct epoll_event));
            evsys.data.ptr = (void*)c->ev;
            evsys.events = EPOLLET | want_opt;
            schk(epoll_ctl(g_event_ctx->epfd, (c->ev->opt == EV_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD), c->fd, &evsys) != -1, return -1);
        }
        c->ev->opt = want_opt;
    }
    return 0;
}

int ev_epoll_loop(time_t msec) {
    int i = 0;

    int all = epoll_wait(g_event_ctx->epfd, g_event_ctx->epev, MAX_NET_CON, (int)msec); 
    if (all <= 0) {
        if (all == 0) {
            return (msec == -1) ? -1 : -11;
        }
        if (errno == EINTR) {
            err("evt epoll_wait irq by [syscall]\n");
            return 0;
        }
        err("evt epoll_wait irq by [err], [%d] [%s]", errno, strerror(errno));
        return -1;
    }
    
    for (i = 0; i < all; i++) {
        ev_t *ev = g_event_ctx->epev[i].data.ptr;
        ev->idxr = ev->idxw = 0;

        int opt = g_event_ctx->epev[i].events;
        if (opt & EV_R) {
            ev->fread = 1;
            ev->idxr = g_event_ctx->evn;
            g_event_ctx->evs[g_event_ctx->evn++] = ev;
        }
        if (opt & EV_W) {
            ev->fwrite = 1;
            ev->idxw = g_event_ctx->evn;
            g_event_ctx->evs[g_event_ctx->evn++] = ev;
        }
    }
    return 0;
}
#else
static int ev_select_init(void) {
    FD_ZERO(&g_event_ctx->rfds);
    FD_ZERO(&g_event_ctx->wfds);
    return 0;
}

static int ev_select_end(void) {
    FD_ZERO(&g_event_ctx->rfds);
    FD_ZERO(&g_event_ctx->wfds);
    return 0;
}

static int ev_select_opt(con_t *c, int want_opt) {
    if (c->ev->opt != want_opt) {
        if (want_opt == (EV_R | EV_W)) {
            if (!(c->ev->opt & EV_R)) 
                FD_SET(c->fd, &g_event_ctx->rfds);
            if (!(c->ev->opt & EV_W))
                FD_SET(c->fd, &g_event_ctx->wfds);
        } else if (want_opt == EV_R) {
            if (c->ev->opt & EV_W)
                FD_CLR(c->fd, &g_event_ctx->wfds);
            if (!(c->ev->opt & EV_R))
                FD_SET(c->fd, &g_event_ctx->rfds);
        } else if (want_opt == EV_W) {
            if (c->ev->opt & EV_R)
                FD_CLR(c->fd, &g_event_ctx->rfds);
            if (!(c->ev->opt & EV_W))
                FD_SET(c->fd, &g_event_ctx->wfds);
        } else { ///EV_NONE
            if (c->ev->opt & EV_W)
                FD_CLR(c->fd, &g_event_ctx->wfds);
            if (c->ev->opt & EV_R)
                FD_CLR(c->fd, &g_event_ctx->rfds);
        }
        c->ev->opt = want_opt;
    }
    return 0;
}

int ev_select_loop(time_t msec) {
    struct timeval wait_tm;
    int fdmax = -1;
    int actall = 0, actn = 0;
    ev_t *ev = NULL;
    con_t *c = NULL;
    queue_t *q = NULL;
    queue_t *n = NULL;
  
    fd_set rfds;
    fd_set wfds;
    memset(&wait_tm, 0, sizeof(struct timeval));
    if (msec > 0) {
        wait_tm.tv_sec = msec/1000;
        wait_tm.tv_usec = (msec%1000)*1000;
    }

    if (!queue_empty(&g_event_ctx->evqueue)) {
        /// find max fd in event queue
        q = queue_head(&g_event_ctx->evqueue); 
        while (q != queue_tail(&g_event_ctx->evqueue)) {
            n = queue_next(q);
            
            ev = ptr_get_struct(q, ev_t, queue);
            c = ev->c;
            if (c->fd > fdmax) 
                fdmax = c->fd;
            q = n;
        }    
    }

    memcpy(&rfds, &g_event_ctx->rfds, sizeof(fd_set)); ///select return will be change read fds and write fds
    memcpy(&wfds, &g_event_ctx->wfds, sizeof(fd_set));
    actall = select(fdmax + 1, &rfds, &wfds, NULL, (msec != -1) ? &wait_tm : NULL);
    if (actall <= 0) {
        if (actall == 0) {
            return (msec == -1) ? -1 : -11;
        }
        if (errno == EINTR) {
            err("evt select irq by [syscall]\n");
            return 0;
        }
        err("evt select irq by [err], [%d] [%s]\n", errno, strerror(errno));
        return -1;
    }
       
    q = queue_head(&g_event_ctx->evqueue);
    while (q != queue_tail(&g_event_ctx->evqueue) && actn < actall) {
        ev = ptr_get_struct(q, ev_t, queue);
        ev->idxr = ev->idxw = 0;

        c = ev->c;
        if (FD_ISSET(c->fd, &rfds)) {
            ev->fread = 1;
            ev->idxr = g_event_ctx->evn;
            g_event_ctx->evs[g_event_ctx->evn++] = ev;
        }
        if (FD_ISSET(c->fd, &wfds)) {
            ev->fwrite = 1;
            ev->idxw = g_event_ctx->evn;
            g_event_ctx->evs[g_event_ctx->evn++] = ev;
        }
        q = queue_next(q);
    }
    return 0;
}
#endif

int ev_opt(con_t *c, int want_opt) {
    return (g_event_handler.opt ? g_event_handler.opt(c, want_opt) : 0);
}

int ev_loop(time_t msec) {
    int i = 0;
    ev_t * ev = NULL;
    con_t * c = NULL;

    g_event_ctx->evn = 0;
    if(g_event_handler.run) g_event_handler.run(msec);
    systime_update();

    for (i = 0; i < g_event_ctx->evn; i++) {
        if (g_event_ctx->evs[i]) {
            
            ev = g_event_ctx->evs[i];
            c = ev->c;

            if (ev->fread) {
                ev->fread = 0;
                if (ev->read_cb)
                    ev->read_cb(c);
                
            } else if (ev->fwrite) {
                ev->fwrite = 0;
                if (ev->write_cb)
                    ev->write_cb(c);
            }
        }
    }
    return 0;
}

int ev_alloc(ev_t **ev) {
    ev_t *nev = mem_pool_alloc(sizeof(ev_t));
    schk(nev, return -1);
    queue_insert_tail(&g_event_ctx->evqueue, &nev->queue);
    *ev = nev;
    return 0;
}

int ev_free(ev_t *ev) {
    if (ev) {
        if (ev->idxr) 
            g_event_ctx->evs[ev->idxr] = NULL;
        if (ev->idxw)
            g_event_ctx->evs[ev->idxw] = NULL;
    
        queue_remove(&ev->queue);
        mem_pool_free(ev);
    }
    return 0;
}

int ev_init(void) {
    schk(!g_event_ctx, return -1);
    g_event_ctx = mem_pool_alloc(sizeof(g_event_t));
    schk(g_event_ctx, return -1);
    queue_init(&g_event_ctx->evqueue);

    ///def select driver
#if defined(EVENT_EPOLL)
    g_event_handler.init = ev_epoll_init;
    g_event_handler.end = ev_epoll_end;
    g_event_handler.opt = ev_epoll_opt;
    g_event_handler.run = ev_epoll_loop;
#else
    g_event_handler.init = ev_select_init;
    g_event_handler.end = ev_select_end;
    g_event_handler.opt = ev_select_opt;
    g_event_handler.run = ev_select_loop;
#endif    
    if (g_event_handler.init)
        g_event_handler.init();

    /// all worker process will be add listen fd into event.
    /// listen fd set by SO_REUSEPORT. 
    /// kernel will be process listen fd loadbalance
    int i = 0;
    for (i = 0; i < 8; i++) {
        if (g_listens[i].fuse) {
            
            schk(0 == net_alloc(&g_listens[i].c), return -1);
            g_listens[i].c->fd = g_listens[i].fd;
            g_listens[i].c->ev->read_cb = net_accept;
            g_listens[i].c->data = &g_listens[i];
            g_listens[i].c->data_cb = NULL;
            ev_opt(g_listens[i].c, EV_R);
        }
    }
    
    return 0;
}

int ev_exit(void) {
    int i = 0;
    for (i = 0; i < 8; i++) {
        if (g_listens[i].fuse)
            net_free(g_listens[i].c);
    }
    
    if (g_event_ctx) {
        if (g_event_handler.end)
            g_event_handler.end();
        ///todo. clear queue data
        mem_pool_free(g_event_ctx);
        g_event_ctx = NULL;
    }
    return 0;
}

#include "common.h"
#include "queue.h"

typedef struct {
    queue_t evqueue;
    ev_t *ev_map[MAX_NET_CON * 2];
    uint32_t ev_mapn;

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
        mem_pool_free(g_event_ctx->epev);
    return 0;
}

static int ev_epoll_opt(con_t *c, int want_opt) {
    /// want type not same as record type
    if (c->ev->opt != want_opt) {
        if (want_opt == EV_NONE) {
            if (-1 == epoll_ctl(g_event_ctx->epfd, EPOLL_CTL_DEL, c->fd, NULL)) {
                err("epoll_ctl c [%p] fd [%d] err. [%d] [%s]\n", c, c->fd, errno, strerror(errno));
                return -1;
            }
        } else {
            struct epoll_event evsys;
            memset(&evsys, 0, sizeof(struct epoll_event));
            evsys.data.ptr = (void *)c->ev;
            evsys.events = EPOLLET | want_opt;
            if (-1 == epoll_ctl(g_event_ctx->epfd, (c->ev->opt == EV_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD), c->fd, &evsys)) {
                err("epoll_ctl c [%p] fd [%d] err. [%d] [%s]\n", c, c->fd, errno, strerror(errno));
                return -1;
            }
        }
        c->ev->opt = want_opt;
    }
    return 0;
}

int ev_epoll_loop(uint64_t ms) {
    int all = epoll_wait(g_event_ctx->epfd, g_event_ctx->epev, MAX_NET_CON, (int)ms);
    if (all <= 0) {
        if (all == 0) {
            return (ms == -1) ? -1 : -11;
        }
        if (errno == EINTR) {
            err("evt epoll_wait irq by [syscall]\n");
            return 0;
        }
        err("evt epoll_wait irq by [err], [%d] [%s]", errno, strerror(errno));
        return -1;
    }

    for (int i = 0; i < all; i++) {
        ev_t *ev = g_event_ctx->epev[i].data.ptr;
        ev->factive = 1;
        ev->idxr = ev->idxw = 0;

        int opt = ev->opt;
        
        if (opt & EV_R) {
            ev->idxr = g_event_ctx->ev_mapn++;
            g_event_ctx->ev_map[ev->idxr] = ev;
            ev->fread = 1;
        }
        if (opt & EV_W) {
            ev->idxw = g_event_ctx->ev_mapn++;
            g_event_ctx->ev_map[ev->idxw] = ev;
            ev->fwrite = 1;
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
        } else { /// EV_NONE
            if (c->ev->opt & EV_W)
                FD_CLR(c->fd, &g_event_ctx->wfds);
            if (c->ev->opt & EV_R)
                FD_CLR(c->fd, &g_event_ctx->rfds);
        }
        c->ev->opt = want_opt;
    }
    return 0;
}

int ev_select_loop(uint64_t ms) {
    
    int fdmax = -1;
    ev_t *ev = NULL;
    queue_t *q = NULL;
    queue_t *n = NULL;

    fd_set rfds = {0};
    fd_set wfds = {0};

    struct timeval tm = {0};
    tm.tv_sec = (time_t)ms / 1000;
    tm.tv_usec = (suseconds_t)((ms % 1000) * 1000);

    if (!queue_empty(&g_event_ctx->evqueue)) {
        /// find max fd in event queue
        q = queue_head(&g_event_ctx->evqueue);
        while (q != queue_tail(&g_event_ctx->evqueue)) {
            n = queue_next(q);
            ev = ptr_get_struct(q, ev_t, queue);
            
            if (ev->c->fd > fdmax) fdmax = ev->c->fd;
            
            q = n;
        }
    }

    memcpy(&rfds, &g_event_ctx->rfds, sizeof(fd_set)); /// select return will be change read fds and write fds
    memcpy(&wfds, &g_event_ctx->wfds, sizeof(fd_set));
    int actall = select(fdmax + 1, &rfds, &wfds, NULL, &tm);
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

    int cont = 0;
    q = queue_head(&g_event_ctx->evqueue);
    while (q != queue_tail(&g_event_ctx->evqueue) && cont < actall) {
        ev = ptr_get_struct(q, ev_t, queue);
        ev->factive = 1;
        ev->idxr = ev->idxw = 0;

        if (FD_ISSET(ev->c->fd, &rfds)) {
            ev->fread = 1;
            ev->idxr = g_event_ctx->ev_mapn++;
            g_event_ctx->ev_map[ev->idxr] = ev;
        }
        if (FD_ISSET(ev->c->fd, &wfds)) {
            ev->fwrite = 1;
            ev->idxw = g_event_ctx->ev_mapn++;
            g_event_ctx->ev_map[ev->idxw] = ev;
        }
        
        q = queue_next(q);
    }
    return 0;
}
#endif

int ev_opt(con_t *c, int want_opt) {
    return (g_event_handler.opt ? g_event_handler.opt(c, want_opt) : 0);
}

int ev_loop(uint64_t ms) {
    g_event_ctx->ev_mapn = 0;
    if (g_event_handler.loop) g_event_handler.loop(ms);
    
    systime_update();
    
    for (int i = 0; i < g_event_ctx->ev_mapn; i++) {
        if (g_event_ctx->ev_map[i]) {
            ev_t *ev = g_event_ctx->ev_map[i];

            if (ev->fread) {
                ev->fread = 0;
                if (ev->read_cb) ev->read_cb(ev->c);
            } else if (ev->fwrite) {
                ev->fwrite = 0;
                if (ev->write_cb) ev->write_cb(ev->c);
            }
        }
    }
    return 0;
}

int ev_alloc(ev_t **ev) {
    ev_t *alloc_ev = mem_pool_alloc(sizeof(ev_t));
    schk(alloc_ev, return -1);
    queue_insert_tail(&g_event_ctx->evqueue, &alloc_ev->queue);
    *ev = alloc_ev;
    return 0;
}

int ev_free(ev_t *ev) {
    if (ev) {
        if (ev->factive) {
            if (ev->idxr >= 0) g_event_ctx->ev_map[ev->idxr] = NULL;
            if (ev->idxw >= 0) g_event_ctx->ev_map[ev->idxw] = NULL;
        }

        queue_remove(&ev->queue);
        mem_pool_free(ev);
    }
    return 0;
}

int ev_init(void) {
    g_event_ctx = mem_pool_alloc(sizeof(g_event_t));
    schk(g_event_ctx, return -1);
    queue_init(&g_event_ctx->evqueue);

    /// def select driver
#if defined(EVENT_EPOLL)
    g_event_handler.init    = ev_epoll_init;
    g_event_handler.end     = ev_epoll_end;
    g_event_handler.opt     = ev_epoll_opt;
    g_event_handler.loop    = ev_epoll_loop;
#else
    g_event_handler.init    = ev_select_init;
    g_event_handler.end     = ev_select_end;
    g_event_handler.opt     = ev_select_opt;
    g_event_handler.loop    = ev_select_loop;
#endif
    
    if (g_event_handler.init)
        g_event_handler.init();

    /// all worker process will be add listen fd into event.
    /// listen fd set by SO_REUSEPORT.
    /// kernel will be process listen fd loadbalance
    for (int i = 0; i < 8; i++) {
        if (g_listens[i].fuse) {
            if (0 == net_alloc(&g_listens[i].c)) {
                g_listens[i].c->fd = g_listens[i].fd;
                g_listens[i].c->ev->read_cb = net_accept;
                g_listens[i].c->data = &g_listens[i];
                g_listens[i].c->data_cb = NULL;
                ev_opt(g_listens[i].c, EV_R);
            } else {
                err("evt alloc listen con err\n");
                return -1;
            }
        }
    }
    return 0;
}

int ev_exit(void) {
    for (int i = 0; i < 8; i++) {
        if (g_listens[i].fuse)
            net_free(g_listens[i].c);
    }

    if (g_event_ctx) {
        if (g_event_handler.end)
            g_event_handler.end();
        /// todo. clear queue data
        mem_pool_free(g_event_ctx);
        g_event_ctx = NULL;
    }
    return 0;
}

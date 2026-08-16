#ifndef _NET_EVENT_H_INCLUDED_
#define _NET_EVENT_H_INCLUDED_

#ifdef __cplusplus
extern "C" {
#endif

struct ev_t{
    queue_t queue;
    void *data;
    
    uint32_t mask;   ///current event option (EV_R, EV_W, EV_NONE)
    uint32_t idxr;
    uint32_t idxw;
    
    uint8_t fread : 1;    /// mark readable, writable
    uint8_t fwrite : 1;
} ;

void ev_wake(void);
int ev_opt(ev_t *event, int fd, uint32_t new_mask);
int ev_loop(void);

int ev_alloc(ev_t **ev);
int ev_free(ev_t *ev);

int ev_init(void);
int ev_exit(void);


#ifdef __cplusplus
}
#endif

#endif


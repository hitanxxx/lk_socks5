#ifndef _LISTEN_H_INCLUDED_
#define _LISTEN_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct listen listen_t;
struct listen {
	unsigned short port;
	int fd;
	ev_cb cb;

	char fuse:1;
	char fssl:1;
};

extern listen_t g_listens[8];


status listen_start(void);
status listen_init(void);
status listen_end(void);


#ifdef __cplusplus
}
#endif
    
#endif

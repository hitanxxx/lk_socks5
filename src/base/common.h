#ifndef _COMMON_H_INCLUDED_
#define _COMMON_H_INCLUDED_


#include "macro.h"
#include "shm.h"
#include "mem.h"
#include "bst.h"
#include "bheap.h"
#include "queue.h"
#include "arr.h"
#include "list.h"
#include "meta.h"
#include "sys_string.h"
#include "sys_cipher.h"
#include "ezhash.h"
#include "sys_time.h"
#include "config.h"
#include "log.h"
#include "process.h"
#include "net_send.h"
#include "timer.h"
#include "event.h"
#include "listen.h"
#include "ssl.h"
#include "net.h"
#include "ringbuffer.h"
#include "cJSON.h"
#include "aho_corasick.h"


/// improve compile performance
#define LIKELY(x)  __builtin_expect(!!(x), 1)
#define UNLIKELY(x)  __builtin_expect(!!(x), 0)

#define schk(x, action) do { if(UNLIKELY(!(x))) { err("schk failed. (%s)\n", #x); action; } } while(0);
#define S5_OVER_TLS

#endif

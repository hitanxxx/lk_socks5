#ifndef _LK_H_INCLUDED_
#define _LK_H_INCLUDED_

#include "l_base.h"

typedef status ( * module_init_pt )(void);
typedef struct modules_init {
	module_init_pt	pt;
	char *			str;
}modules_init_t;
status dynamic_module_init( void );
status dynamic_module_end( void );
status modules_end( void );

#endif

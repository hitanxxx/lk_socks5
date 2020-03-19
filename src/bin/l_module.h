#ifndef L_MODULE_H_INCLUDE
#define L_MODULE_H_INCLUDE

typedef status ( * module_init_pt )(void);
typedef status ( * module_end_pt )(void);
typedef struct modules_init 
{
	module_init_pt	init_pt;
	module_end_pt	end_pt;
	char *			module_name;
}modules_init_t;

status modules_end( void );
status module_init( void );

#endif


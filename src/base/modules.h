#ifndef _MODULES_H_INCLUDE_
#define _MODULES_H_INCLUDE_ 

#ifdef __cplusplus
extern "C"
{
#endif

status modules_process_init( void );
status modules_pocess_exit( void );

status modules_core_init( void );
status modules_core_exit( void );	


#ifdef __cplusplus
}
#endif
        
#endif


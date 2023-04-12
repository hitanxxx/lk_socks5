#ifndef _MODULES_H_INCLUDE_
#define _MODULES_H_INCLUDE_ 

#ifdef __cplusplus
extern "C"
{
#endif

status modules_init( void );
status modules_end( void );

status core_modules_init( void );
status core_modules_end( void );	


#ifdef __cplusplus
}
#endif
        
#endif


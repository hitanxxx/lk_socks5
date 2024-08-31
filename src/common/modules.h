#ifndef _MODULES_H_INCLUDE_
#define _MODULES_H_INCLUDE_ 

#ifdef __cplusplus
extern "C"
{
#endif

int modules_process_init(void);
int modules_pocess_exit(void);

int modules_core_init(void);
int modules_core_exit(void);


#ifdef __cplusplus
}
#endif
        
#endif


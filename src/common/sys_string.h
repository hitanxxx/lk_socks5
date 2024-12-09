#ifndef _SYS_STRING_H_INCLUDED_
#define _SYS_STRING_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define string(str)     {sizeof(str)-1, (unsigned char*)str}
#define string_null     {0, NULL}
#define string_clr(x)   {(x)->len = 0; (x)->data = NULL;}

typedef struct {
    int  len;
    unsigned char *  data;
} string_t;

unsigned char * l_find_str(unsigned char * str, uint32 str_len, unsigned char * find, uint32 find_len);

#ifdef __cplusplus
}
#endif
        
#endif

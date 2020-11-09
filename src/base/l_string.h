#ifndef _L_STRING_H_INCLUDED_
#define _L_STRING_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif
    

#define string(str)     { sizeof(str)-1, (unsigned char*)str }
#define string_null     { 0, NULL }

typedef struct string_t
{
	uint32           len;
	unsigned char *  data;
} string_t;

char * l_strncpy( char * dst, uint32 dst_len, char * src, uint32 src_len );
unsigned char * l_find_str( unsigned char * str, uint32 str_len, unsigned char * find, uint32 find_len );
status l_atoi( unsigned char * str, uint32 length, int32 * num );
status l_atof( unsigned char * str, uint32 length, float * num );
status l_hex2dec( unsigned char * str, uint32 length, int32 * num );
status l_strncmp_cap( unsigned char * src, uint32 src_length, unsigned char * dst, uint32 dst_length );

#ifdef __cplusplus
}
#endif
        
#endif

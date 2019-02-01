#ifndef _L_STRING_H_INCLUDED_
#define _L_STRING_H_INCLUDED_

#define string(str)          { sizeof(str)-1, (char*)str }
#define string_null			 { 0, NULL }

typedef struct string_t {
	uint32 len;
	char * data;
} string_t;

char * l_strncpy( char * dst, uint32 dst_len, char * src, uint32 src_len );
char * l_find_str( char * str, uint32 str_len, char * find, uint32 find_len );
status l_atoi( char * str, uint32 length, int32 * num );
status l_atof( char * str, uint32 length, float * num );
status l_hex2dec( char * str, uint32 length, int32 * num );
status l_strncmp_cap( char * src, uint32 src_length, char * dst, uint32 dst_length );

#endif

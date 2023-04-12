#include "common.h"


unsigned char * l_find_str( unsigned char * str, uint32 str_length, unsigned char * find, uint32 find_length )
{
    unsigned char * p, * s;
    uint32 i, j;

	if( str_length < 1 || find_length < 1 ) {
		return NULL;
	}
	for( i = 0; i < str_length; i ++ ) {
		if( str[i] == find[0] ) {
			p = &str[i];
			s = &find[0];
			for( j = 0; j < find_length; j ++ ) {
				if( *(p+j) != *(s+j) ) {
					break;
				}
			}
			if( j == find_length ) {
				return p;
			}
		}
	}
    return NULL;
}


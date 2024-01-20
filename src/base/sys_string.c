#include "common.h"


unsigned char * l_find_str( unsigned char * str, uint32 strn, unsigned char * find, uint32 findn )
{
    uint32 i, j;

    assert( strn > 0 );
    assert( findn > 0 );
    assert( strn >= findn );

    for( i = 0; i < strn; i ++ ) {
        if( str[i] == find[0] ) {
            for( j = 0; j < findn; j ++ ) {
                if( str[i+j] != find[j] ) {
                    break;
                }
            }

            if( j == findn ) {
                return str+i;
            }
        }
    }
    return NULL;
}


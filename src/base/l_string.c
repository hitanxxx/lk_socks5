#include "l_base.h"

char * l_strncpy( char * dst, uint32 dst_len, char * src, uint32 src_len )
{
	if( dst_len < src_len ) return NULL;

	while( dst_len && src_len ) {
		*dst++ = *src++;
		dst_len --;
		src_len --;
	}
	return dst;
}

// l_pow ------
static uint32 l_pow( uint32 x, uint32 pow )
{
	uint32 rc = 0;
	uint32 time;

	time = pow;

	// fix me
	if( time == 0 ) {
		return 1;
	} else if( time > 0 ) {
		rc = x;
		while( time > 1 ) {
			rc *= x;
			time --;
		}
	}
	return rc;
}
// l_find_str -------------------
char * l_find_str( char * str, uint32 str_length, char * find, uint32 find_length )
{
    char * p, * s;
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
// l_atoi --------------------
status l_atoi( char * str, uint32 length, int32 * num )
{
	char * p;
	uint32 minus = 0;
	int32 rc = 0;

	p = str;
	if( *p == ' ' ) p ++;
	if( *p == '+' || *p == '-' ) {
		if( *p == '-' ) {
			minus = 1;
		}
		p ++;
	}
	while( p < (str + length) ) {
		if( '0' <= *p && *p <= '9' ) {
			rc = 10 * rc + (int32)(*p - '0');
			p ++;
		} else {
			return ERROR;
		}
	}
	*num = minus ? ( 0 - rc ) : rc;
	return OK;
}
// l_atof --------------------------
status l_atof( char * str, uint32 length, float * num )
{
	char * p;
	uint32 minus = 0, pow = 1;
	float interge_rc = 0;
	float decimal_rc = 0;
	string_t 	interge_str = string_null;
	string_t	decimal_str = string_null;

	enum {
		start = 0,
		symbol,
		interge,
		point,
		decimal
	} state;

	// check str
	state = start;
	for( p = str; p < str + length; p ++ ) {
		if( state == start ) {
			if( *p == ' ' ) {
				continue;
			} else if ( *p == '+' ) {
				state = symbol;
				continue;
			} else if ( *p == '-' ) {
				minus = 1;
				state = symbol;
				continue;
			} else if ( '0' <= *p && *p <= '9' ) {
				state = symbol;
			} else {
				return ERROR;
			}
		}
		if( state == symbol ) {
			if ( '0' <= *p && *p <= '9' ) {
				interge_str.data = p;
				state = interge;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == interge ) {
			if( *p == '.' ) {
				interge_str.len = (uint32)( p - interge_str.data );
				state = point;
				continue;
			} else if ( '0' <= *p && *p <= '9' ) {
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == point ) {
			if ( '0' <= *p && *p <= '9' ) {
				decimal_str.data = p;
				state = decimal;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == decimal ) {
			if ( '0' <= *p && *p <= '9' ) {
				continue;
			} else {
				return ERROR;
			}
		}
	}
	if( decimal_str.data ) {
		decimal_str.len = (uint32)(p - decimal_str.data);
	} else {
		if( interge_str.data ) {
			interge_str.len = (uint32)(p - interge_str.data);
		} else {
			return ERROR;
		}
	}
	// make number
	p = interge_str.data;
	while( p < interge_str.data + interge_str.len ) {
		interge_rc = interge_rc * 10 + (float)( *p - '0' );
		p ++;
	}
	if( decimal_str.data ) {
		p = decimal_str.data;
		while( p < decimal_str.data + decimal_str.len ) {
			decimal_rc = decimal_rc + (float)( *p - '0' )/ (float)l_pow( 10, pow );
			pow ++;
			p ++;
		}
	}

	*num = minus ? ( 0 - ( interge_rc + decimal_rc ) ) : ( interge_rc + decimal_rc );
	return OK;
}

// l_hex2dec -------
status l_hex2dec( char * str, uint32 length, int32 * num )
{
	char * p;
	string_t num_string = string_null;
	uint32 minus = 0;
	int32  rc = 0;

	int32 table[] = {
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,1,2,
		3,4,5,6,7,8,9,0,0,0,
		0,0,0,0,10,11,12,13,14,15,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,10,11,12,13,
		14,15,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0
	};

	enum {
		start = 0,
		hex_frist_c,
		hex_second_c,
		hex_value_start,
		hex_value
	} state;

	// check string
	state = start;
	for( p = str; p < str + length; p ++ ) {
		if( state == start ) {
			if( *p == ' ' ) {
				continue;
			} else if( *p == '-' || *p == '+' ) {
				if( *p == '-' ) {
					minus = 1;
				}
				state = hex_frist_c;
				continue;
			} else if ( ( '0' <= *p && *p <= '9' ) || ( 'a' <= *p && *p <= 'f' ) ) {
				state = hex_frist_c;
			} else {
				return ERROR;
			}
		}
		if( state == hex_frist_c ) {
			if ( ( '0' <= *p && *p <= '9' ) || ( 'a' <= *p && *p <= 'f' ) ) {
				num_string.data = p;
				if( *p == '0' ) {
					state = hex_second_c;
					continue;
				} else {
					state = hex_value;
					continue;
				}
			} else {
				return ERROR;
			}
		}
		if( state == hex_second_c ) {
			if( *p == 'x' || *p == 'X' ) {
				state = hex_value_start;
				continue;
			} else if ( ( '0' <= *p && *p <= '9' ) || ( 'a' <= *p && *p <= 'f' ) ) {
				state = hex_value;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == hex_value_start ) {
			if ( ( '0' <= *p && *p <= '9' ) || ( 'a' <= *p && *p <= 'f' ) ) {
				num_string.data = p;
				state = hex_value;
				continue;
			} else {
				return ERROR;
			}
		}
		if( state == hex_value ) {
			if ( ( '0' <= *p && *p <= '9' ) || ( 'a' <= *p && *p <= 'f' ) ) {
				continue;
			} else {
				return ERROR;
			}
		}
	}
	if( num_string.data ) {
		num_string.len = (uint32)( p - num_string.data );
	} else {
		return ERROR;
	}

	// make number
	p = num_string.data;
	while( p < num_string.data + num_string.len ) {
		rc = rc * 16 + table[ (int32)*p - 1 ];
		p ++;
	}
	*num = minus ? ( 0 - rc ) : rc;
	return OK;
}
// l_strncmp_cap ---------
status l_strncmp_cap( char * src, uint32 src_length, char * dst, uint32 dst_length )
{
	uint32 i = 0;

	if( src_length != dst_length ) {
		return ERROR;
	}
	for( i = 0; i < dst_length; i ++ ) {
		if( (src[i] >= 'a' && src[i] <= 'z') || (src[i] >= 'A' && src[i] <= 'Z') ) {
			if( (32 == l_abs( src[i] - dst[i] ) ) || src[i] == dst[i] ) {
				continue;
			} else {
				return ERROR;
			}
		} else {
			if( src[i] != dst[i] ) {
				return ERROR;
			}
		}
	}
	return OK;
}

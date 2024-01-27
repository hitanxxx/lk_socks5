#include "common.h"


static char	time_str_log[128] = {0};
static char time_str_gmt[128] = {0};
static long long	    time_msec;
static char  *arr_week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char  *arr_month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
						"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

char * systime_log( )
{
	return time_str_log;
}

char * systime_gmt( )
{
	return time_str_gmt;
}

long long systime_msec( )
{
	return time_msec;
}

status systime_update( void )
{
	struct timeval tv;
	time_t sec;
	int msec;

	memset( &tv, 0, sizeof(struct timeval) );
	gettimeofday( &tv, NULL );
	sec = tv.tv_sec;
	msec = tv.tv_usec/1000;
	time_msec = sec * 1000 + msec;
	
	struct tm gmt;
	gmtime_r( &sec, &gmt );
	memset( time_str_gmt, 0, sizeof(time_str_gmt) );
	sprintf( (char*)time_str_gmt,
		"%s, %02d %s %04d %02d:%02d:%02d GMT",
		arr_week[gmt.tm_wday],
		gmt.tm_mday,
		arr_month[gmt.tm_mon],
		gmt.tm_year+1900,
		gmt.tm_hour,
		gmt.tm_min,
		gmt.tm_sec
	);
	
	struct tm local;
	localtime_r( &sec, &local );
	local.tm_mon ++;
	local.tm_year += 1900;
	memset( time_str_log, 0, sizeof(time_str_log) );
	sprintf( (char*)time_str_log,
		"%04d/%02d/%02d %02d:%02d:%02d.%03d",
		(unsigned int)(local.tm_year%1000+2000),
		(unsigned int)(local.tm_mon%100),
		(unsigned int)(local.tm_mday%100),
		(unsigned int)(local.tm_hour%100),
		(unsigned int)(local.tm_min%100),
		(unsigned int)(local.tm_sec%100),
		msec
	);
	return OK;
}

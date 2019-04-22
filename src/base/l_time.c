#include "lk.h"

string_t	cache_time_gmt;
string_t	cache_time_log;
int64_t		cache_time_msec;

static char		cache_time_gmt_str[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+1];
static char		cache_time_log_str[sizeof("0000.00.00 00:00:00 ")+1];

static char  *week[] = { "Sun", "Mon", "Tue",  "Wed", "Thu", "Fri", "Sat" };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// l_time_update -----------------------
status l_time_update( void )
{
	struct timeval tv;
	int64_t sec, msec;
	struct tm gmt, local;
	
	gettimeofday( &tv, NULL );
	sec = tv.tv_sec;
	msec = tv.tv_usec/1000;
	cache_time_msec = sec * 1000 + msec;

	gmtime_r( &sec, &gmt );
	snprintf( cache_time_gmt_str, sizeof(cache_time_gmt_str),
		"%s, %02d %s %4d %02d:%02d:%02d GMT",
		week[gmt.tm_wday],
		gmt.tm_mday,
        months[gmt.tm_mon - 1],
		gmt.tm_year,
		gmt.tm_hour,
		gmt.tm_min,
		gmt.tm_sec
	);
	cache_time_gmt.data = cache_time_gmt_str;
	cache_time_gmt.len = l_strlen(cache_time_gmt_str);

	localtime_r( &sec, &local );
	local.tm_mon++;
    local.tm_year += 1900;
	snprintf( cache_time_log_str, sizeof(cache_time_log_str),
		"%04d.%02d.%02d %02d:%02d:%02d ",
		local.tm_year,
		local.tm_mon,
		local.tm_mday,
		local.tm_hour,
		local.tm_min,
		local.tm_sec
	);
	cache_time_log.data = cache_time_log_str;
	cache_time_log.len = l_strlen(cache_time_log_str);

	return OK;
}

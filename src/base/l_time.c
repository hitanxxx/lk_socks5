#include "lk.h"

string_t	cache_time_gmt;
string_t	cache_time_log;
int64_t		cache_time_msec;

static char		cache_time_gmt_str[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+1];
static char		cache_time_log_str[sizeof("[0000/00/00]-[00:00:00]-")+1];

static char  *week[] = { "Sun",
 "Mon",
 "Tue",
 "Wed",
 "Thu",
 "Fri",
 "Sat" };
static char  *months[] = { "Jan",
 "Feb",
 "Mar",
 "Apr",
 "May",
 "Jun",
 "Jul",
 "Aug",
 "Sep",
 "Oct",
 "Nov",
 "Dec" };


// l_time_gmt ------------------------
static status l_time_gmt( int64_t t, struct tm * tp )
{
	int64_t yday, sec, min, hour, mday, mon, year, wday, days, leap;

	if (t < 0) {
        t = 0;
    }

    days = t / 86400;
    sec = t % 86400;
    /*
     * no more than 4 year digits supported,
     * truncate to December 31, 9999, 23:59:59
     */

    if (days > 2932896) {
        days = 2932896;
        sec = 86399;
    }

    /* January 1, 1970 was Thursday */

    wday = (4 + days) % 7;

    hour = sec / 3600;
    sec %= 3600;
    min = sec / 60;
    sec %= 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/core/ngx_parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */

    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);

    yday = days - (365 * year + year / 4 - year / 100 + year / 400);

    if (yday < 0) {
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = (yday + 31) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - (367 * mon / 12 - 30) + 1;

    if (yday >= 306) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }

	tp->tm_sec  = (int)sec;
    tp->tm_min  = (int)min;
    tp->tm_hour = (int)hour;
    tp->tm_mday = (int)mday;
    tp->tm_mon  = (int)mon;
    tp->tm_year = (int)year;
    tp->tm_wday = (int)wday;
	return OK;
}
// l_time_update -----------------------
status l_time_update( void )
{
	struct timeval tv;
	int64_t sec, msec;
	struct tm gmt, local;
	time_t t;
	
	gettimeofday( &tv, NULL );
	sec = tv.tv_sec;
	msec = tv.tv_usec/1000;
	cache_time_msec = sec * 1000 + msec;

	l_time_gmt( sec, &gmt );
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
	cache_time_gmt.len = sizeof(cache_time_gmt_str)-1;

	t = (time_t)sec;
	localtime_r( &t, &local );
	local.tm_mon++;
    local.tm_year += 1900;
	snprintf( cache_time_log_str, sizeof(cache_time_log_str),
		"[%4d/%02d/%02d]-[%02d:%02d:%02d]-",
		local.tm_year,
		local.tm_mon,
		local.tm_mday,
		local.tm_hour,
		local.tm_min,
		local.tm_sec
	);
	cache_time_log.data = cache_time_log_str;
	cache_time_log.len = sizeof(cache_time_log_str)-1;

	return OK;
}

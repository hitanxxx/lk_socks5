#include "common.h"

static char time_str_log[128] = {0};
static char time_str_gmt[128] = {0};
static uint64_t time_msec = 0;

static char *arr_week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *arr_month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *systime_log(void) { return time_str_log; }
char *systime_gmt(void) { return time_str_gmt; }
uint64_t systime_msec(void) { return time_msec; }

void systime_update(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    time_t sec = tv.tv_sec;
    int msec = tv.tv_usec / 1000;
    time_msec = (sec * 1000) + msec;

    struct tm gmt;
    gmtime_r(&sec, &gmt);
    snprintf((char *)time_str_gmt, sizeof(time_str_gmt),
        "%s, %02d %s %04d %02d:%02d:%02d GMT",
        arr_week[gmt.tm_wday], gmt.tm_mday, arr_month[gmt.tm_mon],
        gmt.tm_year + 1900, gmt.tm_hour, gmt.tm_min, gmt.tm_sec);

    struct tm local;
    localtime_r(&sec, &local);
    snprintf((char *)time_str_log, sizeof(time_str_log),
        "%02d/%02d %02d:%02d:%02d.%03d",
        ///local.tm_year + 1900,
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min,
        local.tm_sec,
        msec);
    return;
}

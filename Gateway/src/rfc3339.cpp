#include <Arduino.h>
#include <rfc3339.h>

#define RFC3339_VERSION "0.0.16"
#define DAY_IN_SECS 86400
#define HOUR_IN_SECS 3600
#define MINUTE_IN_SECS 60
#define HOUR_IN_MINS 60


double _gettime(void) {
#if defined(HAVE_GETTIMEOFDAY)
    // => Use gettimeofday() in usec
    struct timeval t;
#if defined(GETTIMEOFDAY_NO_TZ)
    if (gettimeofday(&t) == 0)
        return ((double)t.tv_sec) + ((double)t.tv_usec * 0.000001);
#else
    struct timezone *tz = NULL;
    if (gettimeofday(&t, tz) == 0)
        return ((double)t.tv_sec) + ((double)t.tv_usec * 0.000001);
#endif
#elif defined(HAVE_FTIME)
    // => Use ftime() in msec
    struct timeb t;
    ftime(&t);
    return ((double)t.time) + ((double)t.millitm * 0.001);
#else
    // Fallback to time() in sec
    time_t t;
    time(&t);
    return (double)t;
#endif

    // -Wreturn-type
    return 0.0;
}

/*
 * Convert positive and negative timestamp double to date_time_struct
 * based on gmtime
 */
void _timestamp_to_date_time(double timestamp, date_time_struct *now,
                                    int offset) {
    timestamp += (offset * MINUTE_IN_SECS);

    time_t t = (time_t)timestamp;
    double fraction = (double)((timestamp - (int)timestamp) * 1000000);
    int usec = fraction >= 0.0 ?\
        (int)floor(fraction + 0.5) : (int)ceil(fraction - 0.5);

    if (usec < 0) {
        t -= 1;
        usec += 1000000;
    }

    if (usec == 1000000) {
        t += 1;
        usec = 0;
    }

    struct tm *ts = NULL;
    ts = gmtime(&t);

    (*now).date.year = (*ts).tm_year + 1900;
    (*now).date.month = (*ts).tm_mon + 1;
    (*now).date.day = (*ts).tm_mday;
    (*now).date.wday = (*ts).tm_wday + 1;
    (*now).date.ok = 1;

    (*now).time.hour = (*ts).tm_hour;
    (*now).time.minute = (*ts).tm_min;
    (*now).time.second = (*ts).tm_sec;
    (*now).time.fraction = (int)usec; // sec fractions in microseconds
    (*now).time.offset = offset;
    (*now).time.ok = 1;

    (*now).ok = 1;
}

/*
 * Create date-time with current values (time now) with given timezone offset
 * offset = UTC offset in minutes
 */
#define _now(now, offset) _timestamp_to_date_time(_gettime(), now, offset)

/*
 * Create date-time with current values in UTC
 */
void _utcnow(date_time_struct *now) {
    _now(now, 0);
}

/*
 * Create RFC3339 date-time string
 */
void _format_date_time(date_time_struct *dt, char* datetime_string) {
    int offset = (*dt).time.offset;
    char sign = '+';

    if (offset < 0) {
        offset = offset * -1;
        sign = '-';
    }

    sprintf(
        datetime_string,
        "%04d-%02d-%02dT%02d:%02d:%02d.%06d%c%02d:%02d",
        (*dt).date.year,
        (*dt).date.month,
        (*dt).date.day,
        (*dt).time.hour,
        (*dt).time.minute,
        (*dt).time.second,
        (*dt).time.fraction,
        sign,
        offset / HOUR_IN_MINS,
        offset % HOUR_IN_MINS
    );
}
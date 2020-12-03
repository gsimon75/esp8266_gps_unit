#include "timegm.h"

#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int _DAYS_BEFORE_MONTH[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#define _ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) (_ISLEAP(year) ? 366 : 365)

time_t 
timegm(struct tm *tim_p)
{
    if (tim_p->tm_year > 10000 || tim_p->tm_year < -10000)
        return (time_t) -1;

    /* compute hours, minutes, seconds */
    time_t tim = tim_p->tm_sec + (tim_p->tm_min * _SEC_IN_MINUTE) + (tim_p->tm_hour * _SEC_IN_HOUR);

    /* compute days in current year */
    long days = (tim_p->tm_mday - 1) + _DAYS_BEFORE_MONTH[tim_p->tm_mon];
    if (tim_p->tm_mon > 1 && _DAYS_IN_YEAR(tim_p->tm_year) == 366)
        days++;

    /* compute days in other years */
    int year = tim_p->tm_year;
    if (year > 70)
    {
        for (year = 70; year < tim_p->tm_year; year++)
            days += _DAYS_IN_YEAR (year);
    }
    else if (year < 70)
    {
        for (year = 69; year > tim_p->tm_year; year--)
            days -= _DAYS_IN_YEAR (year);
        days -= _DAYS_IN_YEAR (year);
    }

    /* compute total seconds */
    tim += (time_t)days * _SEC_IN_DAY;

    return tim;
}
// vim: set sw=4 ts=4 indk= et si:

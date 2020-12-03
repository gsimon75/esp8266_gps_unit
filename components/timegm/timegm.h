#ifndef TIMEGM_H
#define TIMEGM_H

#include <time.h>

// https://www.man7.org/linux/man-pages/man3/timegm.3.html
time_t timegm(struct tm *tim_p);

#endif // TIMEGM_H
// vim: set sw=4 ts=4 indk= et si:

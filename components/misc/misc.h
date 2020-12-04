#ifndef MISC_H
#define MISC_H

#include <time.h>
#include <stdint.h>

// https://www.man7.org/linux/man-pages/man3/timegm.3.html
time_t timegm(struct tm *tim_p);
void hexdump(const uint8_t *data, ssize_t len);

#endif // MISC_H
// vim: set sw=4 ts=4 indk= et si:

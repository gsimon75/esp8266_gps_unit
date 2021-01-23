#ifndef MISC_H
#define MISC_H

#include <esp_system.h>
#include <time.h>
#include <stdint.h>

#define ustrchr(s, c)   ((unsigned char*)strchr((const char*)(s), c))

#define _STR(x) #x
#define STR(x) _STR(x)

// https://www.man7.org/linux/man-pages/man3/timegm.3.html
time_t timegm(struct tm *tim_p);
void hexdump(const uint8_t *data, ssize_t len);
void task_info(void);
extern uint32_t idle_counter;
extern const uint32_t source_date_epoch;

void wait_idle(void);
esp_err_t idle_start(void);

#endif // MISC_H
// vim: set sw=4 ts=4 indk= et si:

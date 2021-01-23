#include "misc.h"
#include "main.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/esp_freertos_hooks.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <ctype.h>
#include <stdio.h>

#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int _DAYS_BEFORE_MONTH[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#define _ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) (_ISLEAP(year) ? 366 : 365)

static const char *TAG = "misc";
uint32_t idle_counter = 0;

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


void
hexdump(const uint8_t *data, ssize_t len) {
    size_t offs = 0;
    char linebuf[4 + 2 + 3*16 + 1 + 16 + 1];
    while (len > 0) {
        int i;
        char *p = linebuf;
        p += sprintf(p, "%04x:", offs);
        for (i = 0; (i < len) && (i < 0x10); ++i) {
            p += sprintf(p, " %02x", data[i]);
        }
        for (; i < 0x10; ++i) {
            *(p++) = ' ';
            *(p++) = ' ';
            *(p++) = ' ';
        }
        *(p++) = ' ';
        for (i = 0; (i < len) && (i < 0x10); ++i) {
            *(p++) = isprint(data[i]) ? data[i] : '.'; 
        }
        *(p++) = '\0';
        ESP_LOGD(TAG, "%s", linebuf);
        offs += 0x10;
        data += 0x10;
        len -= 0x10;
    }
}

size_t
heap_available(void) {
    void * p[128];
    size_t result = 0;

    int pidx = 0;
    for (size_t len = 0x100000; len; len >>= 1) {
        while (pidx < 128) {
            p[pidx] = malloc(len);
            if (!p[pidx]) {
                break;
            }
            ++pidx;
            result += len;
        }
    }
    for (int i = 0; i < pidx; ++i) {
        free(p[i]);
    }
    return result;
}

void
task_info(void) {
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    ESP_LOGD(TAG, "num_tasks: %u, idle_counter=%u", num_tasks, idle_counter);
    char *buf = (char*) malloc(48 * num_tasks);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate task info buffer");
        return;
    }
    vTaskList(buf);

    ESP_LOGD(TAG, "Tasks:\n%s", buf);
    free(buf);
    ESP_LOGD(TAG, "Heap free: %u", heap_available());

    /*struct mallinfo mi = mallinfo();
    ESP_LOGD(TAG, "mem heap=%u, hwm=%u, alloc=%u, free=%u", mi.arena, mi.usmblks, mi.uordblks, mi.fordblks);*/
}


static bool
test_idle() {
    ++idle_counter;
    xEventGroupSetBits(main_event_group, IDLE_TASK_ACTIVE);
    return true;
}


void
wait_idle(void) {
    xEventGroupClearBits(main_event_group, IDLE_TASK_ACTIVE);
    xEventGroupWaitBits(main_event_group, IDLE_TASK_ACTIVE, false, true, portMAX_DELAY);
}


esp_err_t
idle_start(void) {
    esp_register_freertos_idle_hook(test_idle);
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

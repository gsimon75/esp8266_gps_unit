#include "gps.h"
#include "line_reader.h"
#include "oled_stdout.h"
#include "timegm.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "gps";

gps_fix_t gps_fix;

#define BUF_SIZE 256
static QueueHandle_t uart0_queue;

static size_t available;

static ssize_t
gps_source(void *arg __attribute__((unused)), unsigned char *buf, size_t len) {
    if (len > available) {
        len = available;
    }
    if (len > 0) {
        uart_read_bytes(UART_NUM_0, buf, len, portMAX_DELAY);
        available -= len;

        buf[len] = '\0';
    }
    return len;
}

static bool
split_by_comma(char *msg, char **field, int last_idx) {
    for (int idx = 0; idx <= last_idx; ++idx) {
        field[idx] = msg;
        char *sep = strchr(msg, ',');
        if (sep) {
            if (idx == last_idx) { // too many fields
                return false;
            }
            *sep = '\0';
            msg = sep + 1;
        }
        else if (idx != last_idx) { // too few fields
            return false;
        }
    }
    return true;
}


static bool
got_gprmc(char *msg) {
    char * field[13];
    gps_fix_t fix;

    ESP_LOGD(TAG, "got GPRMC: '%s'", msg);
    if (!split_by_comma(msg, field, 12)) {
        return false;
    }

    // parse the quality
    fix.is_valid = field[2][0] == 'A';

    if (fix.is_valid) {
        // parse the datetime
        struct tm dt;
        dt.tm_isdst = 0;
        if (4 != sscanf(field[1], "%02d%02d%02d%f", &dt.tm_hour, &dt.tm_min, &dt.tm_sec, &fix.datetime)) {
            return false;
        }
        if (3 != sscanf(field[9], "%02d%02d%02d", &dt.tm_mday, &dt.tm_mon, &dt.tm_year)) {
            return false;
        }
        --dt.tm_mon;
        dt.tm_year += 100;
        fix.datetime += timegm(&dt); // please don't comment. this whole date format is crap right from the beginning

        // parse the location
        int degree;
        float minute;
        if (2 != sscanf(field[3], "%02d%f", &degree, &minute)) {
            return false;
        }
        fix.latitude = degree + (minute / 60.0);
        if (field[4][0] == 'S')
            fix.latitude = -fix.latitude;
        if (2 != sscanf(field[5], "%03d%f", &degree, &minute)) {
            return false;
        }
        fix.longitude = degree + (minute / 60.0);
        if (field[6][0] == 'W')
            fix.longitude = -fix.longitude;

        // parse speed
        if (1 != sscanf(field[7], "%f", &fix.speed_kph)) {
            return false;
        }
        fix.speed_kph *= 1.852; // knots to kph. don't... please just don't. i also would've preferred earth radius per lunar phase cycle as a speed unit...

        // parse azimuth
        if (1 != sscanf(field[8], "%f", &fix.azimuth)) {
            // valid if missing (eg. when standing still), use the last one in this case
            fix.azimuth = gps_fix.azimuth;
        }
    }
    else {
        fix.datetime = 0;
        fix.latitude = 0;
        fix.longitude = 0;
        fix.speed_kph = 0;
        fix.azimuth = 0;
    }


    taskENTER_CRITICAL();
    bool changed =
           (gps_fix.is_valid  != fix.is_valid)
        || (gps_fix.datetime  != fix.datetime)
        || (gps_fix.latitude  != fix.latitude)
        || (gps_fix.longitude != fix.longitude)
        || (gps_fix.speed_kph != fix.speed_kph)
        || (gps_fix.azimuth   != fix.azimuth);
    if (changed) {
        gps_fix.is_valid  = fix.is_valid;
        gps_fix.datetime  = fix.datetime;
        gps_fix.latitude  = fix.latitude;
        gps_fix.longitude = fix.longitude;
        gps_fix.speed_kph = fix.speed_kph;
        gps_fix.azimuth   = fix.azimuth;
        gps_fix.tick      = soc_get_ccount();
    }
    taskEXIT_CRITICAL();

    if (changed) {
        ESP_LOGD(TAG, "v=%d, t=%f, lat=%f, lng=%f, spd=%f, azm=%f", fix.is_valid, fix.datetime, fix.latitude, fix.longitude, fix.speed_kph, fix.azimuth);
        // FIXME: fire an event
        // FIXME: tune the clock
    }

    lcd_gotoxy(0, 2);
    printf("%d, %5.3f, %5.3f, %f", fix.is_valid, fix.latitude, fix.longitude, fix.datetime);
    fflush(stdout);
    return true;
}

static bool
check_nmea(uint8_t *msg) {
    if (!msg || (msg[0] != '$')) {
        return false;
    }
    msg++;

    uint8_t *sep = strchr(msg, '*');
    if (!sep || !sep[1] || !sep[2] || sep[3]) {
        return false;
    }
    uint8_t chksum = 0;
    for (const uint8_t *p = msg; p != sep; ++p) {
        chksum ^= *p;
    }

    uint8_t *p;
    uint8_t shouldbe = strtol(sep + 1, (char**)&p, 16);
    if (*p || (chksum != shouldbe)) {
        return false;
    }
    *sep = '\0';

    if (!strncmp(msg, "GPRMC,", 6)) {
        got_gprmc((char*)msg);
    }
    else {
    }
    return true;
}

static void
uart_event_task(void *pvParameters) {

    line_reader_t *lr = lr_new(BUF_SIZE, gps_source, NULL);

    while (1) {
        uart_event_t event;
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    ESP_LOGV(TAG, "UART_DATA %d bytes", event.size);
                    available = event.size;
                    unsigned char *line;
                    while ((line = lr_read_line(lr))) { 
                        ESP_LOGI(TAG, "%s", line);
                        check_nmea(line);
                    }
                    break;
                }

                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "hw fifo overflow");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "ring buffer full");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "uart parity error");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "uart frame error");
                    break;

                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    lr_free(lr);
    lr = NULL;
    vTaskDelete(NULL);
}

esp_err_t
gps_init(void) {
    gps_fix.tick = soc_get_ccount();
    gps_fix.is_valid = false;
    ESP_LOGD(TAG, "_xt_tick_divisor=%u", _xt_tick_divisor);

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_FIFO_LEN + 1, 0, 100, &uart0_queue, 0);

    xTaskCreate(uart_event_task, "gps", 2048, NULL, 12, NULL);
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

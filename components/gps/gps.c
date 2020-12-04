#include "gps.h"
#include "line_reader.h"
#include "oled_stdout.h"
#include "misc.h"

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
#include <endian.h>

static const char *TAG = "gps";

gps_fix_t gps_fix;

#define BUF_SIZE 256
static QueueHandle_t uart0_queue;

uint8_t buf[BUF_SIZE + 1], *wr = buf;
bool ubx_ready = true;


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

    //ESP_LOGD(TAG, "got GPRMC: '%s'", msg);
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

bool
got_nmea(const uint8_t *msg, size_t len) {
    ESP_LOGD(TAG, "NMEA: '%s'", msg);
    uint8_t chksum = 0, shouldbe;
    const uint8_t *end = msg + len;
    const uint8_t *p;

    ++msg; // skip '$'
    for (p = msg; (p < end) && (*p != '*'); ++p) {
        chksum ^= *p;
    }
    if ((p + 3) != end) { // checksum format invalid
        return false;
    }
    char *err = NULL;
    shouldbe = strtol((const char*)(p + 1), &err, 16);
    if ( *err) { // checksum not hex
        return false;
    }
    if (chksum != shouldbe) {
        ESP_LOGE("NMEA checksum error: is=%02x, shouldbe=%02x", chksum, shouldbe);
        return false;
    }

    if (!strncmp(msg, "GPRMC,", 6)) {
        got_gprmc((char*)msg);
    }

    return true;
}

int
got_ubx(const uint8_t *msg, size_t payload_len) {
    ESP_LOGD(TAG, "UBX: class=0x%02x, id=0x%02x, payload_len=0x%04x", msg[2], msg[3], payload_len);

    uint8_t ck_a = 0, ck_b = 0;
    for (int i = 2; i < (payload_len + 6); ++i) {
           ck_a = ck_a + msg[i];
           ck_b = ck_b + ck_a;
    }
    hexdump(msg + 6, payload_len);
    if ((msg[payload_len + 6] != ck_a) || (msg[payload_len + 7] != ck_b)) {
        printf("UBX checksum error: is=[%02x, %02x], shouldbe=[%02x, %02x]", msg[payload_len + 6], msg[payload_len + 7], ck_a, ck_b);
        return false;
    }

    if (msg[2] == 0x05) { // ACK-ACK or ACK-NAK
        // 05,01 = ACK-ACK, 05,02 = ACK-NAK
        // msg[6] = class, msg[7] = id of the message being acknowledged
        ubx_ready = true; // FIXME: no error handling, go ahead
    }
    return true;
}


static void
got_data(size_t available) {
    //ESP_LOGV(TAG, "UART_DATA %d bytes", available);

    while (available > 0) {

        size_t rdlen =  buf + BUF_SIZE - wr;
        if (rdlen == 0) {
            ESP_LOGE(TAG, "Buffer overflow");
            wr = buf;
            rdlen = BUF_SIZE;
        }

        if (rdlen > available) {
            rdlen = available;
        }

        // read some
        //ESP_LOGD(TAG, "Reading max 0x%x to 0x%x", rdlen, wr - buf);
        uart_read_bytes(UART_NUM_0, wr, rdlen, portMAX_DELAY);
        wr += rdlen;
        available -= rdlen;
        //hexdump(buf, wr - buf);

        // process it
        uint8_t *rd = buf;
        while (rd < wr) {
            //ESP_LOGD(TAG, "Msg loop, wr=0x%x, rd=0x%x", wr - buf, rd - buf);

            if (rd[0] == '$') { // nmea message at the beginning (may be incomplete/invalid)
                uint8_t *pos_crlf = memmem(rd, wr - rd, "\r\n", 2);
                if (pos_crlf == NULL) { // incomplete
                    break;
                }
                // complete
                *pos_crlf = 0;
                got_nmea(rd, pos_crlf - rd);
                rd = pos_crlf + 2;
            }
            else if ((rd[0] == 0xb5) && (rd[1] == 0x62)) { // ubx message at the beginning (may be incomplete/invalid)
                if ((rd + 5) < wr) { // long enough to already contain the length field
                    uint16_t m_len = le16toh(*(uint16_t*)(rd + 4));
                    if ((rd + 8 + m_len) < wr) { // complete
                        got_ubx(rd, m_len);
                        rd += 8 + m_len;
                        continue;
                    }
                }
                // incomplete
                break;
            }
            else { // junk at the beginning
                // try to find the beginning of a recognized message
                uint8_t *pos_nmea = (uint8_t*)memchr(rd, '$', wr - rd);
                uint8_t *pos_ubx = (uint8_t*)memmem(rd, wr - rd, "\xb5\x62", 2);

                if (pos_nmea == NULL) {
                    rd = pos_ubx;
                }
                else if (pos_ubx == NULL) {
                    rd = pos_nmea;
                }
                else if (pos_ubx < pos_nmea) {
                    rd = pos_ubx;
                }
                else {
                    rd = pos_nmea;
                }

                if (rd == NULL) { // all junk, skip it
                    rd = wr;
                    break;
                }
            }
        } // while (rd < wr)

        if (rd < wr) {
            //ESP_LOGD(TAG, "Keeping incomplete 0x%x bytes", wr - rd);
            if (rd != buf) {
                memmove(buf, rd, wr - rd);
                wr = buf + (wr - rd);
            }
        }
        else {
            //ESP_LOGD(TAG, "All processed");
            wr = buf;
        }

    } // while (available > 0)
}


static void
uart_event_task(void *pvParameters) {
    while (1) {
        uart_event_t event;
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    got_data(event.size);
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "Hw fifo overflow");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "Ring buffer full");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity error");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "Frame error");
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown event type %d", event.type);
                    break;
            }
        }
    }
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

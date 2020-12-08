#include "gps.h"
#include "line_reader.h"
#include "oled_stdout.h"
#include "misc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <driver/uart.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <endian.h>

static const char *TAG = "gps";

void ets_update_cpu_frequency(uint32_t ticks_per_us);
extern uint64_t g_esp_os_us;

#define USE_NMEA
//#define USE_UBX

gps_fix_t gps_fix;

#define BUF_SIZE 128
static QueueHandle_t uart0_queue;
static TimerHandle_t baud_rate_watchdog_timer;
static int8_t baud_rate_idx;
static int baud_rates[] = { 9600, 230400, 38400, 57600, 115200, };

static uint8_t buf[BUF_SIZE + 1], *wr = buf;
static struct timeval recv_tv;

static const uint8_t * init_msgs[] = {
    "\xb5\x62\x06\x01\x03\x00\xf0\x00\x00\xfa\x0f", // disable NMEA-GGA
    "\xb5\x62\x06\x01\x03\x00\xf0\x01\x00\xfb\x11", // disable NMEA-GLL
    "\xb5\x62\x06\x01\x03\x00\xf0\x02\x00\xfc\x13", // disable NMEA-GSA
    "\xb5\x62\x06\x01\x03\x00\xf0\x03\x00\xfd\x15", // disable NMEA-GSV
    "\xb5\x62\x06\x01\x03\x00\xf0\x05\x00\xff\x19", // disable NMEA-VTG

#ifdef USE_NMEA
    "\xb5\x62\x06\x01\x03\x00\xf0\x04\x01\xff\x18", // enable NMEA-RMC
#else
    "\xb5\x62\x06\x01\x03\x00\xf0\x04\x00\xfe\x17", // disable NMEA-RMC
#endif // USE_NMEA

#ifdef USE_UBX
    "\xb5\x62\x06\x01\x03\x00\x01\x02\x01\x0e\x47", // enable NAV-POSLLH
    "\xb5\x62\x06\x01\x03\x00\x01\x12\x01\x1e\x67", // enable NAV-VELNED
    "\xb5\x62\x06\x01\x03\x00\x01\x21\x01\x2d\x85", // enable NAV-TIMEUTC
#else
    "\xb5\x62\x06\x01\x03\x00\x01\x02\x00\x0d\x46", // disable NAV-POSLLH
    "\xb5\x62\x06\x01\x03\x00\x01\x12\x00\x1d\x66", // disable NAV-VELNED
    "\xb5\x62\x06\x01\x03\x00\x01\x21\x00\x2c\x84", // disable NAV-TIMEUTC
#endif // !USE_UBX

    // NOTE: leave the baudrate setting for the last, because we'll miss its ACK
    //"\xb5\x62\x06\x00\x00\x00\x06\x18", // poll current port settings
    //"\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x80\x25\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\x8e\x95", // set port1 to 9600,8n1
    //"\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\x96\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\x7f\x70", // set port1 to 34800,8n1
    //"\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\xe1\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\xca\xa9", // set port1 to 57600,8n1
    //"\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\xc2\x01\x00\x03\x00\x03\x00\x00\x00\x00\x00\xac\x5e", // set port1 to 115200,8n1
    "\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\x84\x03\x00\x03\x00\x03\x00\x00\x00\x00\x00\x70\xc8", // set port1 to 230400,8n1
    NULL
};

static const uint8_t **cur_init_msg;


static bool
send_ubx(const uint8_t *msg) {
    size_t len = 8 + le16dec(msg + 4);
    //ESP_LOGV(TAG, "Sending %d bytes", len);
    //hexdump(msg, len);
    int res = uart_write_bytes(UART_NUM_0, msg, len);
    if (res != len) {
        ESP_LOGE(TAG, "Failed to send complete message, sent=%d, len=%d", res, len);
        return false;
    }
    return true;
}


static void
process_new_fix(void) {
    ESP_LOGD(TAG, "New fix; v=%d, t=%.0lf, lat=%f, lng=%f, spd=%f, azm=%f", gps_fix.is_valid, (double)(gps_fix.time_usec / 1e6), gps_fix.latitude, gps_fix.longitude, gps_fix.speed_kph, gps_fix.azimuth);
    // FIXME: fire an event
    // FIXME: tune the clock

    lcd_gotoxy(0, 2);
    printf("%d, %5.3f, %5.3f, %lf", gps_fix.is_valid, gps_fix.latitude, gps_fix.longitude, (double)(gps_fix.time_usec / 1e6));
    fflush(stdout);
}


static void
process_new_time(uint64_t time_usec) {
    static bool first_fix = true;

#ifdef TIME_DRIFT_STATS
    static int64_t drift_total = 0;
    static int drift_n = 0;
    static int drift_max = 0, drift_min = 0;
#endif // TIME_DRIFT_STATS

    static int current_freq = 160;

    int64_t recv_usec = ((int64_t)recv_tv.tv_sec) * 1000000UL + recv_tv.tv_usec;
    int64_t delta_usec = ((int64_t)time_usec) - recv_usec;

    if (__builtin_expect(first_fix, false)) { // no time set yet
        struct timeval tv;
        gettimeofday(&tv, NULL);

        tv.tv_sec += delta_usec / 1000000L;
        tv.tv_usec += delta_usec % 1000000L;

        if (tv.tv_usec < 0) {
            tv.tv_usec += 1000000;
            --tv.tv_sec;
        }
        else if (1000000 <= tv.tv_usec) {
            tv.tv_usec -= 1000000;
            ++tv.tv_sec;
        }

        settimeofday(&tv, NULL);
        ESP_LOGI(TAG, "System time set to %lu", tv.tv_sec);
        first_fix = false;
    }
    else { // normal operation
        int freq;
        if (delta_usec > 500) {
            freq = 159;
        }
        else if (delta_usec < -500) {
            freq = 161;
        }
        else {
            freq = 160;
        }
        if (freq != current_freq) {
            current_freq = freq;
            ets_update_cpu_frequency(freq);
        }

#ifdef TIME_DRIFT_STATS
        drift_total += delta_usec;
        drift_n++;
        if (delta_usec > drift_max) {
            drift_max = delta_usec;
        }
        if (delta_usec < drift_min) {
            drift_min = delta_usec;
        }
        ESP_LOGD(TAG, "new time, dt=%.0lf, drift=%.lf, avg_drift=[%d..%lf..%d], n=%d", (double)time_usec, (double)delta_usec, drift_min, (double)drift_total/drift_n, drift_max, drift_n);
#endif // TIME_DRIFT_STATS

    }
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
got_GPRMC(char *msg) {
    char * field[13];
    gps_fix_t fix;

    ESP_LOGV(TAG, "%s", msg);
    if (!split_by_comma(msg, field, 12)) {
        return false;
    }

    // parse the quality
    fix.is_valid = field[2][0] == 'A';

    if (fix.is_valid) {
        // parse the datetime
        struct tm dt;
        double sec_frac;
        dt.tm_isdst = 0;
        if (4 != sscanf(field[1], "%02d%02d%02d%lf", &dt.tm_hour, &dt.tm_min, &dt.tm_sec, &sec_frac)) {
            return false;
        }
        if (3 != sscanf(field[9], "%02d%02d%02d", &dt.tm_mday, &dt.tm_mon, &dt.tm_year)) {
            return false;
        }
        --dt.tm_mon;
        dt.tm_year += 100;
        sec_frac += timegm(&dt);
        fix.time_usec = 1000000UL * sec_frac; // please don't comment. this whole date format is crap right from the beginning

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
        fix.time_usec = 0;
        fix.latitude = 0;
        fix.longitude = 0;
        fix.speed_kph = 0;
        fix.azimuth = 0;
    }

    if (gps_fix.time_usec != fix.time_usec) {
        process_new_time(fix.time_usec);
    }

    taskENTER_CRITICAL();
    bool changed =
           (gps_fix.is_valid  != fix.is_valid)
        || (gps_fix.time_usec  != fix.time_usec)
        || (gps_fix.latitude  != fix.latitude)
        || (gps_fix.longitude != fix.longitude)
        || (gps_fix.speed_kph != fix.speed_kph)
        || (gps_fix.azimuth   != fix.azimuth);
    if (changed) {
        gps_fix.is_valid  = fix.is_valid;
        gps_fix.time_usec  = fix.time_usec;
        gps_fix.latitude  = fix.latitude;
        gps_fix.longitude = fix.longitude;
        gps_fix.speed_kph = fix.speed_kph;
        gps_fix.azimuth   = fix.azimuth;
    }
    taskEXIT_CRITICAL();
    if (changed) {
        process_new_fix();
    }
    return true;
}


static bool
got_nmea(const uint8_t *msg, size_t len) {
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
        ESP_LOGE(TAG, "NMEA checksum error: is=%02x, shouldbe=%02x", chksum, shouldbe);
        return false;
    }

    xTimerReset(baud_rate_watchdog_timer, 0);
    if (!cur_init_msg) {
        cur_init_msg = init_msgs;
        ESP_LOGI(TAG, "Sending init messages");
        send_ubx(*cur_init_msg);
    }

    if (!strncmp(msg, "GPRMC,", 6)) {
        got_GPRMC((char*)msg);
    }
    else {
        ESP_LOGV(TAG, "NMEA: '%s'", msg);
    }

    return true;
}


static void
got_NAV_POSLLH(const uint8_t *payload) {
    uint32_t iTOW   = le32dec(payload +  0);
    int32_t  lon    = le32dec(payload +  4);
    int32_t  lat    = le32dec(payload +  8);
    int32_t  height = le32dec(payload + 12);
    int32_t  hMSL   = le32dec(payload + 16);
    uint32_t hAcc   = le32dec(payload + 20);
    uint32_t vAcc   = le32dec(payload + 24);
    ESP_LOGV(TAG, "NAV-POSLLG iTOW=%u, lon=%d, lat=%d, height=%d, hMSL=%d, hAcc=%u, vAcc=%u",
        iTOW, lon, lat, height, hMSL, hAcc, vAcc);

    float latitude = lat * 1e-7;
    float longitude = lon * 1e-7;
    taskENTER_CRITICAL();
    bool changed = !gps_fix.is_valid || (gps_fix.latitude != latitude) || (gps_fix.longitude != longitude);
    if (changed) {
        gps_fix.is_valid = true;
        gps_fix.latitude  = latitude;
        gps_fix.longitude = longitude;
    }
    taskEXIT_CRITICAL();
    if (changed) {
        process_new_fix();
    }
}


static void
got_NAV_TIMEUTC(const uint8_t *payload) {
    uint32_t iTOW   = le32dec(payload +  0);
    uint32_t tAcc   = le32dec(payload +  4);
    int32_t  nano   = le32dec(payload +  8);
    uint16_t year   = le32dec(payload + 12);
    uint8_t  month  = payload[14];
    uint8_t  day    = payload[15];
    uint8_t  hour   = payload[16];
    uint8_t  minute = payload[17];
    uint8_t  sec    = payload[18];
    uint8_t  valid  = payload[19];
    ESP_LOGV(TAG, "NAV-TIMEUTC iTOW=%u, tAcc=%u, nano=%d, year=%u, month=%u, day=%u, hour=%u, minute=%d, sec=%u, valid=%u",
        iTOW, tAcc, nano, year, month, day, hour, minute, sec, valid);

    struct tm dt = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = sec,
        .tm_isdst = 0,
    };
    uint64_t time_usec = (1000000UL * timegm(&dt)) + (nano / 1000);

    if (gps_fix.time_usec != time_usec) {
        process_new_time(time_usec);
    }

    taskENTER_CRITICAL();
    bool changed = (gps_fix.time_usec != time_usec);
    if (changed) {
        gps_fix.time_usec  = time_usec;
    }
    taskEXIT_CRITICAL();
    if (changed) {
        process_new_fix();
    }
}


static void
got_NAV_VELNED(const uint8_t *payload) {
    uint32_t iTOW    = le32dec(payload +  0);
    int32_t  velN    = le32dec(payload +  4);
    int32_t  velE    = le32dec(payload +  8);
    int32_t  velD    = le32dec(payload + 12);
    uint32_t speed   = le32dec(payload + 16);
    uint32_t gSpeed  = le32dec(payload + 20);
    int32_t  heading = le32dec(payload + 24);
    uint32_t sAcc    = le32dec(payload + 28);
    uint32_t cAcc    = le32dec(payload + 32);
    ESP_LOGV(TAG, "NAV-VELNED iTOW=%u, velN=%d, velE=%d, velD=%d, speed=%u, gSpeed=%u, heading=%d, sAcc=%u, cAcc=%u",
        iTOW, velN, velE, velD, speed, gSpeed, heading, sAcc, cAcc);

    float speed_kph = speed * 0.036; // cm/s to km/h
    float azimuth = heading * 1e-5;
    taskENTER_CRITICAL();
    bool changed = (gps_fix.speed_kph != speed_kph) || (gps_fix.azimuth != azimuth);
    if (changed) {
        gps_fix.speed_kph = speed_kph;
        gps_fix.azimuth   = azimuth;
    }
    taskEXIT_CRITICAL();
    if (changed) {
        process_new_fix();
    }
}


static int
got_ubx(const uint8_t *msg, size_t payload_len) {
    void
    dump_generic(void) {
        ESP_LOGV(TAG, "UBX: class=0x%02x, id=0x%02x, payload_len=0x%04x", msg[2], msg[3], payload_len);
        hexdump(msg + 6, payload_len);
    }

    uint8_t ck_a = 0, ck_b = 0;
    for (int i = 2; i < (payload_len + 6); ++i) {
           ck_a = ck_a + msg[i];
           ck_b = ck_b + ck_a;
    }
    if ((msg[payload_len + 6] != ck_a) || (msg[payload_len + 7] != ck_b)) {
        printf("UBX checksum error: is=[%02x, %02x], shouldbe=[%02x, %02x]", msg[payload_len + 6], msg[payload_len + 7], ck_a, ck_b);
        dump_generic();
        return false;
    }

    xTimerReset(baud_rate_watchdog_timer, 0);

    switch (msg[2]) {
        case 0x05: // ACK-*
            if (cur_init_msg) {
                if ((msg[6] == (*cur_init_msg)[2]) && (msg[7] == (*cur_init_msg)[3])) {
                    if (msg[3] != 0x01) {
                        ESP_LOGW(TAG, "UBX ACK-NAK for %02x,%02x", msg[6], msg[7]);
                    }
                    else {
                        //ESP_LOGV(TAG, "UBX ACK-ACK for %02x,%02x", msg[6], msg[7]);
                    }
                    if (cur_init_msg[1]) {
                        ++cur_init_msg;
                        send_ubx(*cur_init_msg);
                    }
                    else {
                        ESP_LOGI(TAG, "All init messages acknowledged");
                    }
                }
                else {
                    ESP_LOGW(TAG, "UBX ACK-* for unknown message: got=%02x,%02x, expected=%02x,%02x", msg[6], msg[7], (*cur_init_msg)[2], (*cur_init_msg)[3]);
                }
            }
            break;

        case 0x01: // NAV-*
            switch (msg[3]) {
                case 0x02:
                    got_NAV_POSLLH(msg + 6);
                    break;
                case 0x12:
                    got_NAV_VELNED(msg + 6);
                    break;
                case 0x21:
                    got_NAV_TIMEUTC(msg + 6);
                    break;
                default:
                    dump_generic();
                    break;
            }
            break;

        default:
            dump_generic();
            break;
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
        //ESP_LOGV(TAG, "Reading max 0x%x to 0x%x", rdlen, wr - buf);
        uart_read_bytes(UART_NUM_0, wr, rdlen, portMAX_DELAY);
        wr += rdlen;
        available -= rdlen;
        //hexdump(buf, wr - buf);

        // process it
        uint8_t *rd = buf;
        while (rd < wr) {
            //ESP_LOGV(TAG, "Msg loop, wr=0x%x, rd=0x%x", wr - buf, rd - buf);

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
                    uint16_t m_len = le16dec(rd + 4);
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
            //ESP_LOGV(TAG, "Keeping incomplete 0x%x bytes", wr - rd);
            if (rd != buf) {
                memmove(buf, rd, wr - rd);
                wr = buf + (wr - rd);
            }
        }
        else {
            //ESP_LOGV(TAG, "All processed");
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
                    gettimeofday(&recv_tv, NULL);
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
                    //ESP_LOGE(TAG, "Frame error");
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown event type %d", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

static void
try_next_baud_rate(TimerHandle_t xTimer) {
    baud_rate_idx = (baud_rate_idx + 1) % (sizeof(baud_rates) / sizeof(baud_rates[0]));
    ESP_LOGI(TAG, "Trying baud rate %d", baud_rates[baud_rate_idx]);
    uart_set_baudrate(UART_NUM_0, baud_rates[baud_rate_idx]);
}

esp_err_t
gps_init(void) {
    gps_fix.is_valid = false;
    cur_init_msg = NULL;
    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    baud_rate_idx = -1;
    try_next_baud_rate(NULL);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_FIFO_LEN + 1, 0, 100, &uart0_queue, 0);

    baud_rate_watchdog_timer = xTimerCreate("gps", pdMS_TO_TICKS(3000), pdTRUE, NULL, try_next_baud_rate);
    xTaskCreate(uart_event_task, "gps", 2048, NULL, 12, NULL);
    xTimerStart(baud_rate_watchdog_timer, 0);


    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

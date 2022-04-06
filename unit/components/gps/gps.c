#include "gps.h"
#include "oled_stdout.h"
#include "main.h"
#include "misc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <driver/uart.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <esp_log.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <endian.h>

static const char *TAG = "gps";

void ets_update_cpu_frequency(uint32_t ticks_per_us);
extern uint64_t g_esp_os_us;

//#define USE_NMEA
#define USE_UBX

#define TIMEOUT_SEND_CMD_MS         250
#define TIMEOUT_RECV_ANYTHING_SEC   2

// if the system time is off by more than this, it'll be just set, and not adjusted gradually
#define TIME_SET_GRADUAL_MAX_USEC   1000000

static SemaphoreHandle_t sem_running = NULL;
static bool keep_running = false;
gps_fix_t gps_fix;
gps_status_t gps_status;
const char* gps_status_names[] = {
    " INIT ",
    " NOFIX",
    " TIME ",
    "COARSE",
    "  OK  ",
};

#define BUF_SIZE 128
static QueueHandle_t uart0_queue;

static int baud_rates[] = { 9600, 230400, /* 38400, 57600, 115200, */ }; // possible baud rates that may initially happen

static uint8_t buf[BUF_SIZE + 1], *wr = buf;
static struct timeval recv_tv;

#ifdef USE_NMEA
static time_t last_GPRMC_time;
#endif // USE_NMEA

#ifdef USE_UBX
static time_t last_NAV_POSLLH_time;
static time_t last_NAV_TIMEUTC_time;
static time_t last_NAV_VELNED_time;
#endif // USE_UBX


static void got_ACK(bool is_ack, uint8_t clsID, uint8_t msgID);

static esp_err_t
send_ubx(const uint8_t *msg, unsigned int timeout_ms) {
    size_t len = 8 + le16dec(msg + 4);
    ESP_LOGV(TAG, "Sending UBX %d bytes", len);
    hexdump(msg, len);
    int res = uart_write_bytes(UART_NUM_0, msg, len);
    if (res != len) {
        ESP_LOGE(TAG, "Failed to send complete message, sent=%d, len=%d", res, len);
        return ESP_FAIL;
    }
    /*if (msg[2] == 0x0b) { // AID-* generates no ACK -> fake it
        got_ACK(true, msg[2], msg[3]);
    }*/
    esp_err_t result;
    if (timeout_ms) {
        result = uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(timeout_ms));
    }
    else {
        result = ESP_OK;
    }
    return result;
}


static void
process_new_fix(void) {
    ESP_LOGD(TAG, "New fix; lat=%f, lng=%f, spd=%f, azm=%f", gps_fix.latitude, gps_fix.longitude, gps_fix.speed_kph, gps_fix.azimuth);
    xEventGroupSetBits(main_event_group, GOT_GPS_FIX_BIT);
    //xEventGroupClearBits(main_event_group, GOT_GPS_FIX_BIT);
}


static bool
process_new_time(uint64_t time_usec) {
    if ((time_usec / 1e6) <= source_date_epoch) {
        // runs earlier than the build time -> nonsense
        return false;
    }
    if (gps_status == GPS_NOFIX) {
        gps_status = GPS_TIME;
    }
    if (gps_fix.time_usec == time_usec) {
        return false;
    }

    bool systime_changed = false;

    gps_fix.time_usec = time_usec;
    ESP_LOGD(TAG, "New time %lu", (unsigned long)(time_usec / 1e6));

#ifdef TIME_DRIFT_STATS
    static int64_t drift_total = 0;
    static int drift_n = 0;
    static int drift_max = 0, drift_min = 0;
#endif // TIME_DRIFT_STATS

    static int current_freq = 160;

    int64_t recv_usec = ((int64_t)recv_tv.tv_sec) * 1000000ULL + recv_tv.tv_usec;
    int64_t delta_usec = ((int64_t)time_usec) - recv_usec;

    if ((delta_usec < -TIME_SET_GRADUAL_MAX_USEC) || (TIME_SET_GRADUAL_MAX_USEC < delta_usec)) {
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
        ESP_LOGI(TAG, "System time set to %lu (delta_usec=%f)", tv.tv_sec, (double)delta_usec);
        systime_changed = true;
    }
    else {
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
        ESP_LOGD(TAG, "Drift stat, dt=%.0lf, drift=%.lf, avg_drift=[%d..%lf..%d], n=%d", (double)time_usec, (double)delta_usec, drift_min, (double)drift_total/drift_n, drift_max, drift_n);
#endif // TIME_DRIFT_STATS

    }
    xEventGroupSetBits(main_event_group, GOT_GPS_TIME_BIT);
    //xEventGroupClearBits(main_event_group, GOT_GPS_TIME_BIT);
    return systime_changed;
}

#ifdef USE_NMEA
static void
enable_GPRMC(void) {
    send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x04\x01\xff\x18", TIMEOUT_SEND_CMD_MS); // enable NMEA-RMC
    time(&last_GPRMC_time);
    gps_status = GPS_INIT;
}
#endif // USE_NMEA

#ifdef USE_UBX
static void
enable_NAV_POSLLH(void) {
    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x02\x01\x0e\x47", TIMEOUT_SEND_CMD_MS); // enable NAV-POSLLH
    time(&last_NAV_POSLLH_time);
    gps_status = GPS_INIT;
}

static void
enable_NAV_VELNED(void) {
    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x12\x01\x1e\x67", TIMEOUT_SEND_CMD_MS); // enable NAV-VELNED
    time(&last_NAV_VELNED_time);
    gps_status = GPS_INIT;
}

static void
enable_NAV_TIMEUTC(void) {
    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x21\x01\x2d\x85", TIMEOUT_SEND_CMD_MS); // enable NAV-TIMEUTC
    time(&last_NAV_TIMEUTC_time);
    gps_status = GPS_INIT;
}
#endif // !USE_UBX

static void
reinit_gps(void) {
    gps_status = GPS_INIT;
    for (int baud_rate_idx = 0; baud_rate_idx < (sizeof(baud_rates) / sizeof(baud_rates[0])); ++baud_rate_idx) {
        ESP_LOGI(TAG, "Setting baud rate %d", baud_rates[baud_rate_idx]);
        uart_flush_input(UART_NUM_0);
        uart_set_baudrate(UART_NUM_0, baud_rates[baud_rate_idx]);
        send_ubx("\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\x84\x03\x00\x03\x00\x03\x00\x00\x00\x00\x00\x70\xc8", TIMEOUT_SEND_CMD_MS); // set port1 to 230400,8n1
        // send_ubx("\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x80\x25\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\x8e\x95", TIMEOUT_SEND_CMD_MS); // set port1 to 9600,8n1
        // send_ubx("\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\x96\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\x7f\x70", TIMEOUT_SEND_CMD_MS); // set port1 to 34800,8n1
        // send_ubx("\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\xe1\x00\x00\x03\x00\x03\x00\x00\x00\x00\x00\xca\xa9", TIMEOUT_SEND_CMD_MS); // set port1 to 57600,8n1
        // send_ubx("\xb5\x62\x06\x00\x14\x00\x01\x00\x00\x00\xc0\x08\x00\x00\x00\xc2\x01\x00\x03\x00\x03\x00\x00\x00\x00\x00\xac\x5e", TIMEOUT_SEND_CMD_MS); // set port1 to 115200,8n1
    }
#ifdef USE_NMEA
    enable_GPRMC();
#endif // USE_NMEA

#ifdef USE_UBX
    enable_NAV_POSLLH();
    enable_NAV_VELNED();
    enable_NAV_TIMEUTC();
#endif // !USE_UBX
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


#ifdef USE_NMEA
static bool
got_GPRMC(char *msg) {
    char * field[13];
    float latitude, longitude, speed_kph, azimuth;

    ESP_LOGV(TAG, "%s", msg);
    if (!split_by_comma(msg, field, 12)) {
        return false;
    }

    gps_status = GPS_NOFIX;

    // parse the quality
    bool is_valid = field[2][0] == 'A';

    // parse the datetime
    struct tm dt;
    double sec_frac;
    dt.tm_isdst = 0;
    if ((4 == sscanf(field[1], "%02d%02d%02d%lf", &dt.tm_hour, &dt.tm_min, &dt.tm_sec, &sec_frac))
        && (3 == sscanf(field[9], "%02d%02d%02d", &dt.tm_mday, &dt.tm_mon, &dt.tm_year))
        ) {
        --dt.tm_mon;
        dt.tm_year += 100;
        sec_frac += timegm(&dt);
        process_new_time(1000000ULL * sec_frac); // please don't comment. this whole date format is crap right from the beginning
    }

    // parse the location
    int degree;
    float minute;
    if (2 == sscanf(field[3], "%02d%f", &degree, &minute)) {
        latitude = degree + (minute / 60.0);
        if (field[4][0] == 'S')
            latitude = -latitude;
        gps_status = GPS_OK;
    }
    else {
        latitude = gps_fix.latitude;
    }
    if (2 == sscanf(field[5], "%03d%f", &degree, &minute)) {
        longitude = degree + (minute / 60.0);
        if (field[6][0] == 'W')
            longitude = -longitude;
        gps_status = GPS_OK;
    }
    else {
        longitude = gps_fix.longitude;
    }

    // parse speed
    if (1 == sscanf(field[7], "%f", &speed_kph)) {
        speed_kph *= 1.852; // knots to kph. don't... please just don't. i also would've preferred earth radius per lunar phase cycle as a speed unit...
    }
    else {
        speed_kph = gps_fix.speed_kph;
    }

    // parse azimuth
    if (1 != sscanf(field[8], "%f", &azimuth)) {
        // valid if missing (eg. when standing still), use the last one in this case
        azimuth = gps_fix.azimuth;
    }

    if (is_valid) {
        taskENTER_CRITICAL();
        gps_fix.latitude  = latitude;
        gps_fix.longitude = longitude;
        gps_fix.speed_kph = speed_kph;
        gps_fix.azimuth   = azimuth;
        taskEXIT_CRITICAL();
        process_new_fix();
    }
    time(&last_GPRMC_time);
    return true;
}
#endif // USE_NMEA


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

    if (!strncmp(msg, "GPRMC,", 6)) {
#ifdef USE_NMEA
        got_GPRMC((char*)msg);
#else
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x04\x00\xfe\x17", 0); // disable NMEA-RMC
#endif // USE_NMEA
    }
    else if (!strncmp(msg, "GPGGA,", 6)) {
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x00\x00\xfa\x0f", 0); // disable NMEA-GGA
    }
    else if (!strncmp(msg, "GPGLL,", 6)) {
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x01\x00\xfb\x11", 0); // disable NMEA-GLL
    }
    else if (!strncmp(msg, "GPGSA,", 6)) {
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x02\x00\xfc\x13", 0); // disable NMEA-GSA
    }
    else if (!strncmp(msg, "GPGSV,", 6)) {
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x03\x00\xfd\x15", 0); // disable NMEA-GSV
    }
    else if (!strncmp(msg, "GPVTG,", 6)) {
        send_ubx("\xb5\x62\x06\x01\x03\x00\xf0\x05\x00\xff\x19", 0); // disable NMEA-VTG
    }
    else {
        ESP_LOGV(TAG, "NMEA: '%s'", msg);
    }

    return true;
}


#ifdef USE_UBX
static void
got_NAV_POSLLH(const uint8_t *payload) {
    uint32_t iTOW   = le32dec(payload +  0);
    int32_t  lon    = le32dec(payload +  4);
    int32_t  lat    = le32dec(payload +  8);
    int32_t  height = le32dec(payload + 12);
    int32_t  hMSL   = le32dec(payload + 16);
    uint32_t hAcc   = le32dec(payload + 20);
    uint32_t vAcc   = le32dec(payload + 24);
    ESP_LOGV(TAG, "NAV-POSLLH iTOW=%u, lon=%d, lat=%d, height=%d, hMSL=%d, hAcc=%u, vAcc=%u",
        iTOW, lon, lat, height, hMSL, hAcc, vAcc);

    if (hAcc < 0xffffffff) { // mm
        gps_status = (hAcc < 1000000) ? GPS_OK : GPS_COARSE;
        taskENTER_CRITICAL();
        gps_fix.latitude  = lat * 1e-7;
        gps_fix.longitude = lon * 1e-7;
        taskEXIT_CRITICAL();
        process_new_fix();
    }
    else if (gps_status >= GPS_TIME) {
        gps_status = GPS_TIME;
    }
    else {
        gps_status = GPS_NOFIX;
    }
    time(&last_NAV_POSLLH_time);
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

    if (valid & 0x04) {
        if (gps_status <= GPS_NOFIX) {
            gps_status = GPS_TIME;
        }
        struct tm dt = {
            .tm_year = year - 1900,
            .tm_mon = month - 1,
            .tm_mday = day,
            .tm_hour = hour,
            .tm_min = minute,
            .tm_sec = sec,
            .tm_isdst = 0,
        };
        time_t time_sec = timegm(&dt);
        if (process_new_time((1000000ULL * time_sec) + (nano / 1000))) {
            time(&last_NAV_POSLLH_time);
            time(&last_NAV_VELNED_time);
        }
    }
    time(&last_NAV_TIMEUTC_time);
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

    if ((sAcc < 1000) /* cm/s */ && (cAcc < 25) /* deg */) {
        if (gps_status < GPS_NOFIX) {
            gps_status = GPS_NOFIX;
        }
        float speed_kph = speed * 0.036; // cm/s to km/h
        float azimuth = heading * 1e-5;
        taskENTER_CRITICAL();
        gps_fix.speed_kph = speed_kph;
        gps_fix.azimuth   = azimuth;
        taskEXIT_CRITICAL();
    }
    time(&last_NAV_VELNED_time);
}
#endif // USE_UBX

static void
got_ACK(bool is_ack, uint8_t clsID, uint8_t msgID) {
    if (is_ack) {
        ESP_LOGV(TAG, "UBX ACK-ACK for %02x,%02x", clsID, msgID);
    }
    else {
        ESP_LOGW(TAG, "UBX ACK-NAK for %02x,%02x", clsID, msgID);
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
        ESP_LOGW(TAG, "UBX checksum error: is=[%02x, %02x], shouldbe=[%02x, %02x]", msg[payload_len + 6], msg[payload_len + 7], ck_a, ck_b);
        dump_generic();
        return false;
    }

    switch (msg[2]) {
        case 0x05: // ACK-*
            got_ACK(msg[3] == 0x01, msg[6], msg[7]);
            break;

        case 0x01: // NAV-*
            switch (msg[3]) {
                case 0x02:
#ifdef USE_UBX
                    got_NAV_POSLLH(msg + 6);
#else
                    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x02\x00\x0d\x46", 0); // disable NAV-POSLLH
#endif // USE_UBX
                    break;
                case 0x12:
#ifdef USE_UBX
                    got_NAV_VELNED(msg + 6);
#else
                    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x12\x00\x1d\x66", 0); // disable NAV-VELNED
#endif // USE_UBX
                    break;
                case 0x21:
#ifdef USE_UBX
                    got_NAV_TIMEUTC(msg + 6);
#else
                    send_ubx("\xb5\x62\x06\x01\x03\x00\x01\x21\x00\x2c\x84", 0); // disable NAV-TIMEUTC
#endif // USE_UBX
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
            hexdump(buf, BUF_SIZE);
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
            //hexdump(rd, wr - rd);

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
                    if ((rd + 8 + m_len) <= wr) { // complete
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
    xSemaphoreTake(sem_running, portMAX_DELAY);
    esp_err_t res = uart_driver_install(UART_NUM_0, UART_FIFO_LEN + 1, 0, 100, &uart0_queue, 0);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install uart driver: %d", res);
        goto install_failed;
    }

    reinit_gps();
    ESP_LOGI(TAG, "Serial receiver start");
    for (keep_running = true; keep_running; ) {
        uart_event_t event;
        ESP_LOGD(TAG, "Waiting for uart event");
        if (!xQueueReceive(uart0_queue, (void *)&event, pdMS_TO_TICKS(TIMEOUT_RECV_ANYTHING_SEC * 1000))) {
            ESP_LOGE(TAG, "No event within timeout, reinit");
            //xQueueReset(uart0_queue);
            reinit_gps();
        }
        else {
            switch (event.type) {
                case UART_DATA:
                    ESP_LOGD(TAG, "Got data: %d bytes", event.size);
                    gettimeofday(&recv_tv, NULL);
                    got_data(event.size);
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "Hw fifo overflow");
                    uart_flush_input(UART_NUM_0);
                    continue;

                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "Ring buffer full");
                    uart_flush_input(UART_NUM_0);
                    continue;

                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity error");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "Frame error");
                    break;

                default:
                    ESP_LOGI(TAG, "Unknown event type %d", event.type);
                    break;
            }
            {
                time_t now;
                time(&now);
#ifdef USE_NMEA
                if ((now - last_GPRMC_time) > TIMEOUT_RECV_ANYTHING_SEC) {
                    enable_GPRMC();
                }
#endif // USE_NMEA
#ifdef USE_UBX
                if ((now - last_NAV_POSLLH_time) > TIMEOUT_RECV_ANYTHING_SEC) {
                    enable_NAV_POSLLH();
                }
                if ((now - last_NAV_VELNED_time) > TIMEOUT_RECV_ANYTHING_SEC) {
                    enable_NAV_VELNED();
                }
                if ((now - last_NAV_TIMEUTC_time) > TIMEOUT_RECV_ANYTHING_SEC) {
                    enable_NAV_TIMEUTC();
                }
#endif // USE_UBX
            }
        }
    }
    uart_driver_delete(UART_NUM_0);
install_failed:
    xSemaphoreGive(sem_running);
    vTaskDelete(NULL);
}

esp_err_t
gps_start(void) {
    if (!sem_running) {
        sem_running = xSemaphoreCreateBinary();
        xSemaphoreGive(sem_running);
    }
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    BaseType_t res = xTaskCreate(uart_event_task, "gps", 2048, NULL, 12, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task; res=%d", res);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Started");
    return ESP_OK;
}

esp_err_t
gps_add_agps(const uint8_t *data, size_t datalen) {
    // NOTE: this is called from the lrep task
    ESP_LOGI(TAG, "Processing AGPS data %d bytes", datalen);

#ifdef USE_AGPS
    // split data into UBX command list, only validate and count the commands first
    const uint8_t *rd = data, *end = data + datalen;
    while (rd < end) {
        if (end < (rd + 8)) {
            ESP_LOGE(TAG, "Incomplete UBX message at the end of AGPS data, offset=%u", rd - data);
            return ESP_FAIL;
        }
        if ((rd[0] != 0xb5) || (rd[1] != 0x62)) {
            ESP_LOGE(TAG, "Invalid UBX signature in AGPS data, offset=%u", rd - data);
            return ESP_FAIL;
        }
        uint16_t m_len = le16dec(rd + 4);
        if (end < (rd + 8 + m_len)) {
            ESP_LOGE(TAG, "Incomplete UBX message at the end of AGPS data, offset=%u", rd - data);
            return ESP_FAIL;
        }

        send_ubx(rd, TIMEOUT_SEND_CMD_MS);
        vTaskDelay(pdMS_TO_TICKS(500));

        rd += 8 + m_len;
    }
    ESP_LOGI(TAG, "AGPS data sent");
#endif // USE_AGPS
    return ESP_OK;
}

esp_err_t
gps_stop(void) {
    if (keep_running) {
        keep_running = false;
        xSemaphoreTake(sem_running, portMAX_DELAY);
        xSemaphoreGive(sem_running);
    }
    gps_status = GPS_INIT;
    ESP_LOGI(TAG, "Stopped");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

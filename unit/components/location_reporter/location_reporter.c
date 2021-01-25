#include "main.h"
#include "gps.h"
#include "misc.h"
#include "https_client.h"
#include "oled_stdout.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <esp_spi_flash.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include <driver/adc.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#include <string.h>
#include <ctype.h>
#include <math.h>

#define BODY_MAX 128
#define R_Earth 6.371009e6

static const char *TAG = "lrep";

static char *DATA_SERVER_NAME, *DATA_SERVER_PORT, *DATA_PATH, *DATA_ENDPOINT;
static uint32_t unit_nonce;
static char unit_name[32] = "<unnamed>";
static int unit_status = 0; // FIXME: not handled yet
static char * unit_status_str[] = {
    "Ready",        // 0
    "In use",       // 1
    "Charging",     // 2
    "Repairs",      // 3
    // ...
};

static SemaphoreHandle_t sem_running = NULL;
static bool keep_running = false;
static uint16_t adc_mV;

static void
init_status(void) {
    lcd_clear();
    lcd_puts(11, 0, unit_name);
    lcd_puts(11, 1, unit_status_str[unit_status]);
    //lcd_puts(11, 2, "");
    //lcd_puts(11, 3, "+123.4567");
    //lcd_puts(11, 4, " -12.3456");
    //lcd_puts(11, 5, "2021-01-02");
    //lcd_puts(11, 6, "11:22:33");
    //lcd_puts(11, 7, "U=1.234V");
    uint8_t buf[64];
    size_t len = snprintf((char*)buf, sizeof(buf), "gpsunit://%s/%u", unit_name, unit_nonce);
    lcd_qr(buf, len);
}

static void
show_status(void) {
    time_t tt;
    struct tm t;
    char buf[12];

    time(&tt);
    if (gps_fix.is_valid) {
        gmtime_r(&tt, &t);
   
        snprintf(buf, sizeof(buf), "%+10.5f", gps_fix.latitude);
        lcd_puts(11, 3, buf);
        snprintf(buf, sizeof(buf), "%+10.5f", gps_fix.longitude);
        lcd_puts(11, 4, buf);
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        lcd_puts(11, 5, buf);
        snprintf(buf, sizeof(buf), " %02u:%02u:%02u " ,t.tm_hour, t.tm_min, t.tm_sec);
        lcd_puts(11, 6, buf);
        snprintf(buf, sizeof(buf), "U: %4u mV", adc_mV);
        lcd_puts(11, 7, buf);
    }
    else {
        lcd_puts(11, 3, "  no gps  ");
        lcd_puts(11, 4, "  signal  ");
        if (source_date_epoch < tt) {
            gmtime_r(&tt, &t);
            snprintf(buf, sizeof(buf), "%04u-%02u-%02u", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
            lcd_puts(11, 5, buf);
            snprintf(buf, sizeof(buf), " %02u:%02u:%02u " ,t.tm_hour, t.tm_min, t.tm_sec);
            lcd_puts(11, 6, buf);
        }
        else {
            lcd_puts(11, 5, " no valid ");
            lcd_puts(11, 6, "   time   ");
        }
        snprintf(buf, sizeof(buf), "U: %4u mV", adc_mV);
        lcd_puts(11, 7, buf);
    }
}

void
location_reporter_task(void * pvParameters __attribute__((unused))) {
        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    xSemaphoreTake(sem_running, portMAX_DELAY);
        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    esp_err_t res;
    https_conn_context_t ctx;
    char *url = NULL;
    uint16_t time_trshld = 0, dist_trshld = 0;
    TickType_t time_trshld_ticks = portMAX_DELAY;
    float dist_trshld_deg2 = 40; // > (2*pi)**2

    char *body = (char*)malloc(BODY_MAX);
    if (!body) {
        ESP_LOGE(TAG, "Out of memory");
        printf("LRep mem error\n");
        goto error;
    }

    {
        nvs_handle nvs;
        res = nvs_open("server", NVS_READONLY, &nvs);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Cannot find persistent LRep config: %d", res);
            printf("LRep NVS error\n");
            goto error;
        }
        else {
            size_t url_len;
            res = nvs_get_str(nvs, "url", NULL, &url_len);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Cannot find LRep URL in persistent config: %d", res);
                printf("LRep URL error\n");
                nvs_close(nvs);
                goto error;
            }
            url = (char*)malloc(url_len + 1);
            if (!url) {
                ESP_LOGE(TAG, "Out of memory");
                printf("LRep mem error\n");
                nvs_close(nvs);
                goto error;
            }
            nvs_get_str(nvs, "url", url, &url_len);
            url[url_len] = '\0';

            res = nvs_get_u16(nvs, "time_trshld", &time_trshld);
            if (res != ESP_OK) {
                ESP_LOGW(TAG, "Cannot read LRep time threshold: %d", res);
            }
            else {
                time_trshld_ticks = 1000UL * time_trshld / portTICK_PERIOD_MS;
                ESP_LOGD(TAG, "Time threshold: %u sec = %u ticks", time_trshld, time_trshld_ticks);
            }
            res = nvs_get_u16(nvs, "dist_trshld", &dist_trshld);
            if (res != ESP_OK) {
                ESP_LOGW(TAG, "Cannot read LRep distance threshold: %d", res);
            }
            else {
                dist_trshld_deg2 = 180.0 / M_PI * dist_trshld / R_Earth;
                dist_trshld_deg2 *= dist_trshld_deg2;
                ESP_LOGD(TAG, "Distance threshold: %u m = %e deg", dist_trshld, dist_trshld_deg2);
            }

            ESP_LOGI(TAG, "URL (len=%d) '%s'", url_len, url);
            nvs_close(nvs);
        }
    }

        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    if (!https_split_url(url, &DATA_SERVER_NAME, &DATA_SERVER_PORT, &DATA_PATH, &DATA_ENDPOINT)) {
        ESP_LOGE(TAG, "Won't report location to insecure destination");
        printf("LRep security error\n");
        goto error;
    }

    {
        adc_config_t cfg = {
            .mode = ADC_READ_TOUT_MODE,
            .clk_div = 32,
        };

        res = adc_init(&cfg);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open ADC: %d", res);
        }
    }

        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    xEventGroupSetBits(main_event_group, LREP_RUNNING_BIT);

    ESP_LOGI(TAG, "Connecting to LRep server, name='%s', port='%s', path='%s', endpoint='%s'",
        DATA_SERVER_NAME, DATA_SERVER_PORT, DATA_PATH, DATA_ENDPOINT);

    if (!https_init(&ctx)) {
        ESP_LOGE(TAG, "Cannot set up SSL");
        goto error;
    }

    bool connected = https_connect(&ctx, DATA_SERVER_NAME, DATA_SERVER_PORT);

    {
        // try to find the CN (oid=2.5.4.3, [ 0x55, 0x04, 0x03 ]) from the subject DN
        mbedtls_x509_name *nn = &ctx.client_cert.subject;
        unit_name[0] = '\0';
        while (nn) {
            /*
            ESP_LOGD(TAG, "oid: tag=0x%x, len=%d", nn->oid.tag, nn->oid.len); hexdump(nn->oid.p, nn->oid.len);
            ESP_LOGD(TAG, "val: tag=0x%x, len=%d", nn->val.tag, nn->val.len); hexdump(nn->val.p, nn->val.len);
            ESP_LOGD(TAG, "next_merged=%d", nn->next_merged);
            */
            if ((nn->oid.len == 3) && (nn->oid.p[0] == 0x55) && (nn->oid.p[1] == 0x04) && (nn->oid.p[2] == 0x03)) {
                size_t len = nn->val.len;
                if (len >= sizeof(unit_name)) {
                    ESP_LOGE(TAG, "Unit CN too long: %d", len);
                }
                memcpy(unit_name, nn->val.p, len);
                unit_name[len] = '\0';
                break;
            }
            nn = nn->next;
        }
        if (!unit_name[0]) {
            strncpy(unit_name, "<unknown>", sizeof(unit_name));
        }
    }
    unit_nonce = esp_random();
    ESP_LOGI(TAG, "Unit name='%s', nonce=0x%08x", unit_name, unit_nonce);

#ifdef USE_AGPS
    // fetch the AGPS data and send it to gps
    task_info();
    ESP_LOGI(TAG, "Fetching AGPS data");
    {
        uint8_t *agps_data = NULL;
        do {
            if (!connected) {
                ESP_LOGI(TAG, "Reconnecting to LRep server");
                if (!https_connect(&ctx, DATA_SERVER_NAME, DATA_SERVER_PORT)) {
                    continue;
                }
                connected = true;
            }
            if (!https_send_request(&ctx, "GET", DATA_SERVER_NAME, DATA_PATH, "agps", "Connection: keep-alive\r\n")) {
                // couldn't send: conn closed?, reconnect, retry
                ESP_LOGW(TAG, "Send failed, reconnect");
                https_disconnect(&ctx);
                connected = false;
            }
            else {
                int status = https_read_statusline(&ctx);
                while (https_read_header(&ctx, NULL, NULL)) {
                    // FIXME: handle "Connection: close"
                }
                ESP_LOGD(TAG, "AGPS data length: %d", ctx.content_length);

                uint8_t *chunk, *p;
                size_t chunk_len;
                if ((200 <= status) && (status < 300)) {
                    agps_data = (uint8_t*)realloc(agps_data, ctx.content_length);
                    p = agps_data;
                }
                else {
                    p = NULL;
                }
                while (https_read_body_chunk(&ctx, &chunk, &chunk_len)) {
                    if (p) {
                        memcpy(p, chunk, chunk_len);
                        p += chunk_len;
                    }
                }

                if (status < 100) {
                    // couldn't receive: conn closed?, reconnect, retry
                    ESP_LOGW(TAG, "Recv failed, reconnect");
                    https_disconnect(&ctx);
                    connected = false;
                }
                else if ((200 <= status) && (status < 300)) {
                    // success, done
                    ESP_LOGI(TAG, "Got AGPS data, len=%u", ctx.content_length);
                    //if (agps_data) {
                    //    gps_add_agps(agps_data, ctx.content_length);
                    //}
                }
                else if ((400 <= status) && (status < 600)) {
                    ESP_LOGE(TAG, "AGPS data refused: %d", status);
                }
            }
        } while (!connected);
        if (agps_data) {
            free(agps_data);
            agps_data = NULL;
        }
    }
#endif // USE_AGPS

    float last_latitude = 90.0, last_longitude = 0;
    time_t last_time = 0;

    init_status();
    for (keep_running = true; keep_running; ) {
        EventBits_t uxBits = xEventGroupWaitBits(main_event_group, GOT_GPS_FIX_BIT | GOT_GPS_TIME_BIT, false, false, time_trshld_ticks);

        {
            uint16_t raw_adc;
            res = adc_read(&raw_adc);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read ADC: %d", res);
                adc_mV = 0;
            }
            else {
                adc_mV = (((unsigned int)raw_adc) * 3116) >> 10; // NOTE: it should be 3300 mV, but it isn't, and CONFIG_ESP_PHY_INIT_DATA_VDD33_CONST doesn't change anything
                ESP_LOGE(TAG, "ADC raw=%u, U=%u mV", raw_adc, adc_mV);
            }
        }

        show_status();

        unsigned int bodylen = 0;
        if ((uxBits & GOT_GPS_FIX_BIT) && gps_fix.is_valid) {
            time_t tt = gps_fix.time_usec / 1e6;

            // check if the time limit is exceeded
            bool do_send = time_trshld && ((last_time + time_trshld) < tt);
            if (!do_send) {
                // check if the distance is more than the threshold
                // NOTE: not the correct formula (https://en.wikipedia.org/wiki/Great-circle_distance#Computational_formulas)
                // but only an approximation
                float delta_lat = gps_fix.latitude - last_latitude;
                float delta_lon = gps_fix.longitude - last_longitude;
                float delta_square = delta_lat * delta_lat + delta_lon * delta_lon;
                if (delta_square > dist_trshld_deg2) {
                    do_send = true;
                }
                else {
                    ESP_LOGD(TAG, "Not far enough; d2=%e, trshld=%e", delta_square, dist_trshld_deg2);
                }
            }

            if (do_send) {
                // Coordinate precision: https://xkcd.com/2170/
                // { unit: \"test-1\", time: 1608739445000, lat: 25.04, lon: 55.25, alt: 30.11, battery: 3278.123 }
                last_latitude = gps_fix.latitude;
                last_longitude = gps_fix.longitude;
                last_time = tt;
                bodylen = snprintf(body, BODY_MAX - 1, "{\"unit\":\"%s\",\"nonce\":%u,\"time\":%lu,\"lat\":%.4f,\"lon\":%.4f,\"azi\":%.0f,\"spd\":%.0f,\"bat\":%u}",
                    unit_name, unit_nonce, (unsigned long)(gps_fix.time_usec / 1e6), gps_fix.latitude, gps_fix.longitude, gps_fix.azimuth, gps_fix.speed_kph, adc_mV);
            }
        }
        else {
            time_t tt;
            time(&tt);
            if (source_date_epoch < tt) {
                if (time_trshld && ((last_time + time_trshld) < tt)) {
                    last_time = tt;
                    bodylen = snprintf(body, BODY_MAX - 1, "{\"unit\":\"%s\",\"nonce\":%u,\"time\":%lu,\"bat\":%u}", unit_name, unit_nonce, tt, adc_mV);
                }
            }
            else {
                bodylen = snprintf(body, BODY_MAX - 1, "{\"unit\":\"%s\",\"nonce\":%u,\"bat\":%u}", unit_name, unit_nonce, adc_mV);
            }
        }

        if (bodylen == 0) {
            continue;
        }
        //ESP_LOGD(TAG, "Body (len=%d):\n%s", bodylen, body);
        //task_info();

        do {
            if (!connected) {
                ESP_LOGI(TAG, "Reconnecting to LRep server");
                if (!https_connect(&ctx, DATA_SERVER_NAME, DATA_SERVER_PORT)) {
                    // couldn't connect: drop this report, try again with the next one
                    break;
                }
                connected = true;
            }
            if (!https_send_request(&ctx, "POST", DATA_SERVER_NAME, DATA_PATH, DATA_ENDPOINT, "Connection: keep-alive\r\nContent-Type: application/json\r\nContent-Length: %d\r\n", bodylen)
                || !https_send_data(&ctx, body, bodylen)
                ) {
                // couldn't send: conn closed?, reconnect, retry
                ESP_LOGW(TAG, "Send failed, reconnect");
                https_disconnect(&ctx);
                connected = false;
            }
            else {
                int status = https_read_statusline(&ctx);
                ESP_LOGD(TAG, "Report status: %d", status);
                while (https_read_header(&ctx, NULL, NULL)) {
                    // FIXME: handle "Connection: close"
                }
                while (https_read_body_chunk(&ctx, NULL, NULL)) {
                }

                if (status < 100) {
                    // couldn't receive: conn closed?, reconnect, retry
                    ESP_LOGW(TAG, "Recv failed, reconnect");
                    https_disconnect(&ctx);
                    connected = false;
                }
                else if ((200 <= status) && (status < 300)) {
                    // success, done
                }
                else if ((400 <= status) && (status < 600)) {
                    // 4xx: re-sending the same data wouldn't help
                    // 5xx: Theoretically it would make sense to re-send the same data, so when the server-side error is resolved, it could get
                    // through, but it would be obsolete then, so it's better just to drop this report and try again with the next one
                    ESP_LOGE(TAG, "Data report refused: %d", status);
                }
            }
        } while (!connected);
    }
    if (connected) {
        https_disconnect(&ctx);
    }
    https_destroy(&ctx);

error:
    ESP_LOGI(TAG, "Location reporting stopped");

    if (body) {
        free(body);
        body = NULL;
    }

    if (url) {
        free(url);
        url = NULL;
    }

    xEventGroupClearBits(main_event_group, LREP_RUNNING_BIT);
    xSemaphoreGive(sem_running);
    vTaskDelete(NULL);
}

esp_err_t
location_reporter_start(void) {
    if (!sem_running) {
        sem_running = xSemaphoreCreateBinary();
        xSemaphoreGive(sem_running);
    }
    xEventGroupClearBits(main_event_group, LREP_RUNNING_BIT);
    BaseType_t res = xTaskCreate(location_reporter_task, "lrep", 6 * 1024, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task; res=%d", res);
        return ESP_FAIL;
    }
    xEventGroupWaitBits(main_event_group, LREP_RUNNING_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Started");
    return ESP_OK;
}

esp_err_t
location_reporter_stop(void) {
    if (keep_running) {
        keep_running = false;
        xSemaphoreTake(sem_running, portMAX_DELAY);
        xSemaphoreGive(sem_running);
    }
    ESP_LOGI(TAG, "Stopped");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

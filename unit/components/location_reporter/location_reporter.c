#include "main.h"
#include "gps.h"
#include "misc.h"
#include "https_client.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_spi_flash.h>
#include <esp_ota_ops.h>
#include <nvs.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#include <string.h>
#include <ctype.h>

static const char *TAG = "lrep";

static char *DATA_SERVER_NAME, *DATA_SERVER_PORT, *DATA_PATH, *DATA_ENDPOINT;

void
location_reporter_task(void * pvParameters __attribute__((unused))) {
    esp_err_t res;
    nvs_handle nvs;
    https_conn_context_t ctx;
    char *url = NULL;

    char *body = (char*)malloc(256);
    if (!body) {
        ESP_LOGE(TAG, "Out of memory");
        printf("LRep mem error\n");
        goto error;
    }

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
        ESP_LOGI(TAG, "LRep URL (len=%d) '%s'", url_len, url);
        nvs_close(nvs);
    }

    if (!https_split_url(url, &DATA_SERVER_NAME, &DATA_SERVER_PORT, &DATA_PATH, &DATA_ENDPOINT)) {
        ESP_LOGE(TAG, "Won't report location to insecure destination");
        printf("LRep security error\n");
        goto error;
    }

    bool connected = false;
    char unit_name[32];

    xEventGroupSetBits(main_event_group, LREP_RUNNING_BIT);

    ESP_LOGI(TAG, "Connecting to LRep server, name='%s', port='%s', path='%s', endpoint='%s'",
        DATA_SERVER_NAME, DATA_SERVER_PORT, DATA_PATH, DATA_ENDPOINT);

    if (https_conn_init(&ctx, DATA_SERVER_NAME, DATA_SERVER_PORT)) {
        connected = true;
    }
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
    ESP_LOGI(TAG, "Unit CN: %s", unit_name);

    while (true) {
        xEventGroupWaitBits(main_event_group, GOT_GPS_FIX_BIT, false, true, portMAX_DELAY);
        // Coordinate precision: https://xkcd.com/2170/
        // { unit: \"test-1\", time: 1608739445000, lat: 25.04, lon: 55.25, alt: 30.11, battery: 100.0 }
        int bodylen = snprintf(body, sizeof(body) - 1, "{unit:\"%s\",time:%llu,lat:%.4f,lon:%.4f,azi:%.0f,spd:%.0f,bat:%d}",
            unit_name, gps_fix.time_usec / 1000, gps_fix.latitude, gps_fix.longitude, gps_fix.azimuth, gps_fix.speed_kph, 100);
        ESP_LOGD(TAG, "%s", body);

        int status = 0;
        while (true) {
            if (!connected) {
                ESP_LOGI(TAG, "Reconnecting to LRep server");
                if (!https_conn_init(&ctx, DATA_SERVER_NAME, DATA_SERVER_PORT)) {
                    // couldn't connect: drop this report, try again with the next one
                    break;
                }
                connected = true;
            }
            if (!https_send_request(&ctx, "GET", DATA_SERVER_NAME, DATA_PATH, DATA_ENDPOINT, "Connection: keep-alive\r\nContent-Length: %d\r\n", bodylen)
                || !https_send_data(&ctx, body, bodylen)
                ) {
                // couldn't send: conn closed?, reconnect, retry
                https_conn_destroy(&ctx);
                connected = false;
                continue;
            }
            status = https_read_statusline(&ctx);
            if ((200 <= status) && (status < 300)) {
                // success, done
                // FIXME: (unless 204) should read the body or close the conn
                break;
            }
            if ((400 <= status) && (status < 400)) {
                // re-sending the same data wouldn't help
                ESP_LOGE(TAG, "Data report refused: %d", status);
                break;
            }
            if ((500 <= status) && (status < 600)) {
                // Theoretically it would make sense to re-send the same data, so when the server-side error is resolved, it could get
                // through, but it would be obsolete then, so it's better just to drop this report and try again with the next one
                ESP_LOGW(TAG, "Data report refused: %d", status);
                break;
            }
        }
        // NOTE: if we want to do something depending on the status, we can
    }
    //https_conn_destroy(&ctx);

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
    vTaskDelete(NULL);
}

// vim: set sw=4 ts=4 indk= et si:

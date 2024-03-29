#include "ota.h"
#include "main.h"
#include "misc.h"
#include "https_client.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_spi_flash.h>
#include <esp_ota_ops.h>
#include <nvs.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

#include <string.h>
#include <ctype.h>

static const char *TAG = "ota";

static char *OTA_SERVER_NAME, *OTA_SERVER_PORT, *OTA_PATH, *OTA_DESCRIPTOR_FILENAME;

#define LOG_PARTITION(name, p) ESP_LOGI(TAG, name " partition: address=0x%08x, size=0x%08x, type=%d, subtype=%d, label='%s'", (p)->address, (p)->size, (p)->type, (p)->subtype, (p)->label);

//ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);

void
ota_check_task(void * pvParameters __attribute__((unused))) {
    time_t last_time, now;
    esp_err_t res;

    ESP_LOGI(TAG, "Checking OTA");
    printf("Checking OTA...\r");

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    //LOG_PARTITION("Update", update);

    https_conn_context_t ctx;
    
    char *url = NULL;

    nvs_handle nvs;

    res = nvs_open("ota", NVS_READONLY, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent OTA config: %d", res);
        printf("OTA NVS error\n");
        goto close_conn;
    }
    else {
        size_t url_len;
        res = nvs_get_str(nvs, "url", NULL, &url_len);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Cannot find OTA URL in persistent config: %d", res);
            printf("OTA URL error\n");
            nvs_close(nvs);
            goto close_conn;
        }
        url = (char*)malloc(url_len + 1);
        if (!url) {
            ESP_LOGE(TAG, "Out of memory");
            printf("OTA mem error\n");
            nvs_close(nvs);
            goto close_conn;
        }
        nvs_get_str(nvs, "url", url, &url_len);
        url[url_len] = '\0';
        ESP_LOGI(TAG, "OTA URL (len=%d) '%s'", url_len, url);
        nvs_close(nvs);
    }

    if (!https_split_url(url, &OTA_SERVER_NAME, &OTA_SERVER_PORT, &OTA_PATH, &OTA_DESCRIPTOR_FILENAME)) {
        ESP_LOGE(TAG, "Won't accept firmware from insecure source");
        printf("OTA security error\n");
        goto close_conn;
    }

    ESP_LOGI(TAG, "Connecting to OTA server, name='%s', port='%s', path='%s', descriptor='%s'",
        OTA_SERVER_NAME, OTA_SERVER_PORT, OTA_PATH, OTA_DESCRIPTOR_FILENAME);

    if (!https_init(&ctx) || !https_connect(&ctx, OTA_SERVER_NAME, OTA_SERVER_PORT)) {
        goto close_conn;
    }

    // https content-related data

    int status;
    unsigned char *name, *value;
    unsigned char *data;
    size_t datalen;

    char fw_name[32];
    fw_name[0] = '\0';
    uint16_t fw_checksum = 0;
    size_t fw_size = 0;
    time_t fw_mtime = 1;

    // get the descriptor file first

    ESP_LOGI(TAG, "Getting OTA descriptor");
#ifdef MULTI_IMAGES
    snprintf(fw_name, sizeof(fw_name), OTA_DESCRIPTOR_FILENAME, update->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
    if (!https_send_request(&ctx, "GET", OTA_SERVER_NAME, OTA_PATH, fw_name, NULL)) {
        goto close_conn;
    }
#else
    if (!https_send_request(&ctx, "GET", OTA_SERVER_NAME, OTA_PATH, OTA_DESCRIPTOR_FILENAME, NULL)) {
        goto close_conn;
    }
#endif // MULTI_IMAGES

    status = https_read_statusline(&ctx);
    if (status != 200) {
        ESP_LOGE(TAG, "Response error %d", status);
        goto close_conn;
    }

    while (https_read_header(&ctx, &name, &value)) {
        //ESP_LOGD(TAG, "Header line; name='%s', value='%s'", name, value);
    }
    //ESP_LOGD(TAG, "OTA descriptor length: %u", ctx.content_length);

    while (https_read_header(&ctx, &name, &value)) { // it's the body, but also in "name: value" format
        //ESP_LOGD(TAG, "Body line; name='%s', value='%s'", name, value);
        if (!strcmp(name, "name")) {
            strncpy(fw_name, value, sizeof(fw_name));
        }
        else if (!strcmp(name, "mtime")) {
            fw_mtime = strtol((const char*)value, NULL, 0);
        }
        else if (!strcmp(name, "size")) {
            fw_size = strtol((const char*)value, NULL, 0);
        }
        else if (!strcmp(name, "fletcher16")) {
            fw_checksum = strtol((const char*)value, NULL, 0);
        }
    }
    ESP_LOGI(TAG, "OTA descriptor end");

    if (fw_mtime <= source_date_epoch) {
        ESP_LOGI(TAG, "Current firmware is up-to-date");
        printf("Firmware up-to-date\n");
    }
    else if (fw_name[0]) { // get the firmware binary
        ESP_LOGI(TAG, "Current firmware: %u, available: %lu", source_date_epoch, fw_mtime);
        uint16_t sum1, sum2; // Fletcher16: don't want to deal with odd-bytes-long chunks

        ESP_LOGI(TAG, "Getting OTA binary '%s'", fw_name);
        if (!https_send_request(&ctx, "GET", OTA_SERVER_NAME, OTA_PATH, fw_name, NULL)) {
            goto close_conn;
        }

        int status = https_read_statusline(&ctx);
        if (status != 200) {
            ESP_LOGE(TAG, "Response error %d", status);
            goto close_conn;
        }

        unsigned char *name, *value;
        while (https_read_header(&ctx, &name, &value)) {
            //ESP_LOGD(TAG, "Header line; name='%s', value='%s'", name, value);
        }
        if (ctx.content_length != fw_size) {
            ESP_LOGE(TAG, "OTA binary size mismatch; is=%u, shouldbe=%u", ctx.content_length, fw_size);
            goto close_conn;
        }

        esp_ota_handle_t update_handle = 0 ;
        res = esp_ota_begin(update, fw_size, &update_handle);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", res);
            goto close_conn;
        }

        sum1 = sum2 = 0;
        time(&last_time);
        printf("Downloading firmware\r");
        while (https_read_body_chunk(&ctx, &data, &datalen)) {
            time(&now);
            if ((now - last_time) > 1) {
                ESP_LOGD(TAG, "Body chunk; len=%d, remaining=%u", datalen, ctx.content_remaining);
                last_time = now;
            }
            //hexdump(data, datalen);
            res = esp_ota_write(update_handle, data, datalen);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed, error=0x%x", res);
                goto close_conn;
            }

            // calculate a Fletcher16 on download: if it doesn't match, then the file itself is invalid on
            // the server, so it makes no sense to retry downloading/flashing
            for (size_t i = 0; i < datalen; ++i) {
                sum1 = (sum1 + data[i]) % 255;
                sum2 = (sum2 + sum1) % 255;
            }
        }
        sum1 |= sum2 << 8;

        ESP_LOGD(TAG, "Finishing flashing");
        res = esp_ota_end(update_handle);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed, error=0x%x", res);
            goto close_conn;
        }
        ESP_LOGD(TAG, "Finished flashing");
        if (sum1 != fw_checksum) {
            ESP_LOGE(TAG, "OTA downloaded checksum mismatch; is=0x%04x, shouldbe=0x%04x", sum1, fw_checksum);
            goto close_conn;
        }

        ESP_LOGD(TAG, "Calculating flashed checksum");
        sum1 = sum2 = 0;
        ssize_t current_page = -1;
        for (ssize_t i = 0; i < fw_size; ++i) {
            ssize_t page = i / HTTPS_CLIENT_BUFSIZE;
            if (page != current_page) {
                res = spi_flash_read(update->address + (page * HTTPS_CLIENT_BUFSIZE), ctx.buf, HTTPS_CLIENT_BUFSIZE);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "flash read failed, error=0x%x", res);
                    goto close_conn;
                }
                current_page = page;
            }
            sum1 = (sum1 + ctx.buf[i % HTTPS_CLIENT_BUFSIZE]) % 255;
            sum2 = (sum2 + sum1) % 255;
        }
        sum1 |= sum2 << 8;

        if (sum1 != fw_checksum) {
            ESP_LOGE(TAG, "OTA flashed checksum mismatch; is=0x%04x, shouldbe=0x%04x", sum1, fw_checksum);
            goto close_conn;
        }

        ESP_LOGD(TAG, "Setting boot partition");
        res = esp_ota_set_boot_partition(update);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed, error=0x%x", res);
            goto close_conn;
        }

        ESP_LOGI(TAG, "OTA binary end, restarting");
        printf("Firmware update OK\n");
        printf("Restarting...\n");
        esp_restart();
    }

close_conn:
    if (url) {
        free(url);
        url = NULL;
    }
    https_disconnect(&ctx);
    https_destroy(&ctx);

    ESP_LOGI(TAG, "Finished");
    xEventGroupSetBits(main_event_group, OTA_CHECK_DONE_BIT);
    vTaskDelete(NULL);
}

esp_err_t
ota_check_start(void) {
    BaseType_t res = xTaskCreate(ota_check_task, "ota", 6 * 1024, NULL, 5, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task; res=%d", res);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Started");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

#include "main.h"
#include "ssd1306.h"
#include "ota.h"
#include "oled_stdout.h"
#include "gps.h"
#include "admin_mode.h"
#include "location_reporter.h"
#include "button.h"
#include "misc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <rom/ets_sys.h>
#include <driver/gpio.h>

#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_tls.h>
#include <nvs_flash.h>

#include <driver/adc.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "main";

EventGroupHandle_t main_event_group;

static esp_err_t
event_handler(void *ctx, system_event_t *event) {
    system_event_info_t *info = &event->event_info;
    
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START: {
            esp_wifi_connect();
            break;
        }

        case SYSTEM_EVENT_STA_GOT_IP: {
            const char *ip_s = ip4addr_ntoa(&info->got_ip.ip_info.ip);
            printf("IP:%s\n", ip_s);
            ESP_LOGI(TAG, "STA_GOT_IP %s", ip_s);
            xEventGroupSetBits(main_event_group, WIFI_CONNECTED_BIT);
            ota_check_start();
            break;
        }

        case SYSTEM_EVENT_STA_DISCONNECTED: {
            switch (info->disconnected.reason) {
                case WIFI_REASON_BASIC_RATE_NOT_SUPPORT: {
                    esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N); // Switch to 802.11 bgn mode
                    break;
                }

                case WIFI_REASON_NO_AP_FOUND: {
                    ESP_LOGE(TAG, "WiFi AP not found (bad SSID?)");
                    break;
                }
                 
                case WIFI_REASON_AUTH_FAIL: {
                    ESP_LOGE(TAG, "WiFi auth failed (bad password?)");
                    break;
                }

                default: {
                    ESP_LOGE(TAG, "STA_DISCONNECTED %d", info->disconnected.reason);
                    break;
                }
            }
            esp_wifi_connect();
            xEventGroupClearBits(main_event_group, WIFI_CONNECTED_BIT);
            break;
        }

        default:
            ESP_LOGI(TAG, "Unhandled event 0x%x", event->event_id);
            break;
    }
    return ESP_OK;
}

static bool
wifi_init_sta(void) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_restore());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_event_loop_set_cb(event_handler, NULL);

    nvs_handle nvs_wifi;
    wifi_config_t wifi_config = { };

    esp_err_t res = nvs_open("wifi", NVS_READONLY, &nvs_wifi);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent WiFi config: %d", res);
        printf("NVS structure error\n");
        return false;
    }

    size_t ssid_len = sizeof(wifi_config.sta.ssid);
    res = nvs_get_str(nvs_wifi, "ssid", (char*)wifi_config.sta.ssid, &ssid_len);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find WiFi SSID in persistent config: %d", res);
        printf("NVS SSID error\n");
        return false;
    }
    ESP_LOGI(TAG, "WiFi ssid (len=%d) '%s'", ssid_len, wifi_config.sta.ssid);

    size_t password_len = sizeof(wifi_config.sta.password);
    res = nvs_get_str(nvs_wifi, "password", (char*)wifi_config.sta.password, &password_len);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find WiFi password in persistent config: %d", res);
        printf("NVS password error\n");
        return false;
    }
    ESP_LOGI(TAG, "WiFi password (len=%d)", password_len); // wifi_config.sta.password

    nvs_close(nvs_wifi);

    res = esp_wifi_set_mode(WIFI_MODE_STA);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot set WiFi station mode: %d", res);
        printf("STA mode error\n");
        return false;
    }

    res = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot configure WiFi station: %d", res);
        printf("STA config failed\n");
        return false;
    }

    res = esp_wifi_start();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot start WiFi station: %d", res);
        printf("STA start failed\n");
        return false;
    }

    ESP_LOGI(TAG, "WiFi station started");
    return true;
}

#ifdef SCREEN_TEST
static void
screen_test(void) {

    // Binary pattern to page 7 (== rows 56..63)
    ssd1306_set_range(SSD1306_I2C, 0x00, 0x7f, 7, 7);
    for (uint16_t i = 0; i < 0x80; ++i) {
        ssd1306_send_data_byte(SSD1306_I2C, i);
    }
    // Sine pattern to page 0..6 (== rows 0..55)
    ssd1306_set_range(SSD1306_I2C, 0x00, 0x7f, 0, 6);
    for (int page = 0; page <= 6; ++page) {
        for (int i = 0; i < 0x80; ++i) {
            int y = (int)(28 + (27 * sin(i * 3.1415926 / 64)));
            y -= (page << 3);
            if ((0 <= y) && (y < 8)) {
                ssd1306_send_data_byte(SSD1306_I2C, 1 << y);
            }
            else {
                ssd1306_send_data_byte(SSD1306_I2C, 0);
            }
        }
    }

    lcd_puts(5, 1, "Hello World!");
    lcd_gotoxy(0, 2);
    for (int i = 0; i < 3; ++i) {
        printf("%3d\n", i);
    }

    lcd_clear();
    lcd_qr("https://en.wikipedia.org/wiki/ESP8266", -1);
}
#endif // SCREEN_TEST

/*
extern uint16_t test_tout();
static void
dump_adc(TimerHandle_t xTimer) {
    uint16_t adc_value;
    esp_err_t res = adc_read(&adc_value);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %d", res);
        adc_value = 0;
    }
    uint16_t x = test_tout(0);
    ESP_LOGD(TAG, "adc=%u tout=%u", adc_value, x);
} */

void
app_main()
{
    ESP_LOGI(TAG, "main start");
    idle_start();
    ssd1306_init(SSD1306_I2C, 5, 4);
    lcd_init(SSD1306_I2C);

#ifdef SCREEN_TEST
    screen_test();
#endif // SCREEN_TEST
   
    printf("FW %u\n", source_date_epoch);

    /* Print chip information */
    {
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        ESP_LOGI(TAG, "Cores: %d, Flash: %d MB", chip_info.cores, spi_flash_get_chip_size() / (1024 * 1024));
        printf("Cores:%d, Flash:%dMB\n", chip_info.cores, spi_flash_get_chip_size() >> 20);
    }

    //Initialize NVS
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGW(TAG, "NVS was not initialized");
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    if (res != ESP_OK) {
        printf("NVS error %d\n", res);
        ESP_LOGW(TAG, "NVS failed: %d", res);
    }

    main_event_group = xEventGroupCreate();
    res = esp_event_loop_init(NULL, NULL);
    if (res != ESP_OK) {
        printf("EventLoop error %d\n", res);
        ESP_LOGE(TAG, "Event loop failed: %d", res);
    }

    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    res = esp_wifi_init(&cfg);
    if (res != ESP_OK) {
        printf("WiFi init error %d\n", res);
        ESP_LOGE(TAG, "WiFi init failed: %d", res);
    }
    res = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "WiFi storage failed: %d", res);
    }

    button_start(); // for admin mode

    if (wifi_init_sta()) {
        xEventGroupWaitBits(main_event_group, OTA_CHECK_DONE_BIT, false, true, portMAX_DELAY);
        wait_idle();

ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
        location_reporter_start();
        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
        wait_idle();
        ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    }

    ESP_LOGI(TAG, "Up and running");
    printf("Ready\n");
    gps_start();

    /*{
        adc_config_t cfg = {
            .mode = ADC_READ_TOUT_MODE,
            .clk_div = 32,
        };

        res = adc_init(&cfg);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open ADC: %d", res);
        }
        TimerHandle_t adc_dump_timer;
        adc_dump_timer = xTimerCreate("boo", pdMS_TO_TICKS(500), pdTRUE, NULL, dump_adc);
        xTimerStart(adc_dump_timer, 0);
    }*/

    ESP_LOGI(TAG, "main done");
    task_info();
}
// vim: set sw=4 ts=4 indk= et si:

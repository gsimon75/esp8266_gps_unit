#include "main.h"
#include "ssd1306.h"
#include "ota.h"
#include "oled_stdout.h"
#include "gps.h"
#include "admin_mode.h"
#include "location_reporter.h"
#include "misc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/esp_freertos_hooks.h>
#include <rom/ets_sys.h>
#include <driver/gpio.h>

#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_tls.h>
#include <nvs_flash.h>

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
            xTaskCreate(ota_check_task, "ota", 6 * 1024, NULL, 5, NULL);
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

static void
wifi_stop(void) {
    esp_event_loop_set_cb(NULL, NULL);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
}


// ------------------------------------------------------------------------------


static xQueueHandle gpio_evt_queue = NULL;
static TimerHandle_t gpio_debounce_timer[GPIO_NUM_MAX] = { NULL };

static void
gpio_isr_handler(void *arg) {
    if (arg) {
        xTimerResetFromISR((TimerHandle_t)arg, 0);
    }
}


static void
gpio_debounce_expired(TimerHandle_t xTimer) {
    uint8_t gpio_num = (uintptr_t)pvTimerGetTimerID(xTimer);
    if (gpio_num < GPIO_NUM_MAX) {
        if (gpio_get_level(gpio_num)) {
            gpio_num |= 0x80;
        }
        xQueueSend(gpio_evt_queue, &gpio_num, 0);
    }
}

static bool in_admin_mode = false;

static void
gpio_process_task(void *arg)
{
    while (true) {
        uint8_t io_num;
        if (!xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            break;
        }
        ESP_LOGI(TAG, "GPIO[%d]=%d", io_num & 0x7f, io_num & 0x80);
        switch (io_num) {
            case GPIO_BUTTON: {
                if (in_admin_mode) {
                    break;
                }
                ESP_LOGI(TAG, "Entering admin mode");
                in_admin_mode = true;
                // TODO: stop normal operation
                wifi_stop();
                admin_mode_start();
                break;
            }
        }
    }
    ESP_LOGE(TAG, "GPIO queue error");
    vTaskDelete(NULL);
}


static void
gpio_debouncer_add(int gpio_num, int debounce_ms) {
    if (gpio_debounce_timer[gpio_num]) {
        xTimerChangePeriod(gpio_debounce_timer[gpio_num], pdMS_TO_TICKS(debounce_ms), 0);
    }
    else {
        gpio_debounce_timer[gpio_num] = xTimerCreate("gpio", pdMS_TO_TICKS(debounce_ms), pdFALSE, (void*)gpio_num, gpio_debounce_expired);
        gpio_isr_handler_add(gpio_num, gpio_isr_handler, gpio_debounce_timer[gpio_num]);
    }
}


static void
gpio_debouncer_remove(int gpio_num) {
     gpio_isr_handler_remove(gpio_num);
     xTimerDelete(gpio_debounce_timer[gpio_num], 0);
     gpio_debounce_timer[gpio_num] = NULL;
}


static void
button_init() {
    gpio_evt_queue = xQueueCreate(GPIO_NUM_MAX, sizeof(uint8_t));
    xTaskCreate(gpio_process_task, "gpio", 512, NULL, 10, NULL);
    gpio_install_isr_service(0);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1 << GPIO_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);
    gpio_debouncer_add(GPIO_BUTTON, 300);
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


void
app_main()
{
    ESP_LOGI(TAG, "main start");
    esp_register_freertos_idle_hook(test_idle);

    ssd1306_init(SSD1306_I2C, 4, 5);
    lcd_init(SSD1306_I2C);

#ifdef SCREEN_TEST
    screen_test();
#endif // SCREEN_TEST
   
    printf("FW %u\n", SOURCE_DATE_EPOCH);

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

    tcpip_adapter_init();
    res = esp_event_loop_init(NULL, NULL);
    if (res != ESP_OK) {
        printf("EventLoop error %d\n", res);
        ESP_LOGE(TAG, "Event loop failed: %d", res);
    }

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

    button_init(); // for admin mode

    if (wifi_init_sta()) {
        xEventGroupWaitBits(main_event_group, OTA_CHECK_DONE_BIT, false, true, portMAX_DELAY);
        wait_idle();

        xTaskCreate(location_reporter_task, "lrep", 6 * 1024, NULL, 5, NULL);
        xEventGroupWaitBits(main_event_group, LREP_RUNNING_BIT, false, true, portMAX_DELAY);
        wait_idle();
    }

    ESP_LOGI(TAG, "Up and running");
    printf("Ready\n");
    gps_init();
    ESP_LOGI(TAG, "main done");
    task_info();
}
// vim: set sw=4 ts=4 indk= et si:

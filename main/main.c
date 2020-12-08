#include "main.h"
#include "ssd1306.h"
#include "ota.h"
#include "oled_stdout.h"
#include "gps.h"
#include "admin_mode.h"
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

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "main";

EventGroupHandle_t wifi_event_group;

static esp_err_t
event_handler(void *ctx, system_event_t *event) {
    system_event_info_t *info = &event->event_info;
    
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START: {
            esp_wifi_connect();
            break;
        }

        case SYSTEM_EVENT_STA_GOT_IP: {
            ESP_LOGI(TAG, "STA_GOT_IP %s", ip4addr_ntoa(&info->got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            //xTaskCreate(check_ota, "ota", 12 * 1024, NULL, 5, NULL);
            break;
        }

        case SYSTEM_EVENT_STA_DISCONNECTED: {
            ESP_LOGE(TAG, "STA_DISCONNECTED %d", info->disconnected.reason);
            if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
                esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N); // Switch to 802.11 bgn mode
            }
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        }

        default:
            break;
    }
    return ESP_OK;
}

static void
wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_wifi_restore());
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_event_loop_set_cb(event_handler, NULL);

    nvs_handle nvs_wifi;
    wifi_config_t wifi_config = { };

    size_t ssid_len = sizeof(wifi_config.sta.ssid);
    size_t password_len = sizeof(wifi_config.sta.password);

    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &nvs_wifi));
    ESP_ERROR_CHECK(nvs_get_str(nvs_wifi, "ssid", (char*)wifi_config.sta.ssid, &ssid_len));
    ESP_ERROR_CHECK(nvs_get_str(nvs_wifi, "password", (char*)wifi_config.sta.password, &password_len));
    nvs_close(nvs_wifi);

    ESP_LOGI(TAG, "wifi ssid (len=%d) '%s'", ssid_len, wifi_config.sta.ssid);
    ESP_LOGI(TAG, "wifi password (len=%d)", password_len); // wifi_config.sta.password

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

static void
wifi_stop(void) {
    esp_event_loop_set_cb(NULL, NULL);
    ESP_ERROR_CHECK(esp_wifi_stop());
}


// ------------------------------------------------------------------------------


static xQueueHandle gpio_evt_queue = NULL;

static void
gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void
gpio_task_example(void *arg)
{
    uint32_t io_num;

    while (true) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}


static void
button_init() {
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = 1 << GPIO_BUTTON;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_BUTTON, gpio_isr_handler, (void *) GPIO_BUTTON);
    //gpio_isr_handler_remove(GPIO_BUTTON);
}


void
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
    fflush(stdout);

    /*
    lcd_clear();
    lcd_qr("https://en.wikipedia.org/wiki/ESP8266", -1);
    */
}


void
app_main()
{
    ESP_LOGI(TAG, "start");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP8266 chip with %d CPU cores, WiFi, silicon revision %d, %dMB %s flash\n",
        chip_info.cores, chip_info.revision, spi_flash_get_chip_size() / (1024 * 1024),
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ssd1306_init(SSD1306_I2C, 4, 5);
    lcd_init(SSD1306_I2C);

    screen_test();

    wifi_init_sta();
    //xEventGroupWaitBits(wifi_event_group, OTA_CHECK_DONE_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "real start");
    gps_init();
    button_init();

    //xTaskCreate(admin_mode, "admin", 12 * 1024, NULL, 5, NULL);

    // esp_restart();
}
// vim: set sw=4 ts=4 indk= et si:

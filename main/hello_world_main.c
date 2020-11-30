#include "ssd1306.h"
#include "qrcodegen.h"
#include "font6x8.h"
#include "dns_server.h"
#include "ota.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <rom/ets_sys.h>
#include <driver/gpio.h>

#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

#include <esp_tls.h>

#include <nvs_flash.h>

#include <stdio.h>
#include <string.h>

#define GPIO_BUTTON     0
#define SSD1306_I2C I2C_NUM_0

static const char *TAG = "main";

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event - are we connected to the AP with an IP? */
#define WIFI_CONNECTED_BIT  BIT0
#define GPS_GOT_FIX_BIT  BIT1

static esp_err_t
event_handler(void *ctx, system_event_t *event) {
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;
    
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START: {
            esp_wifi_connect();
            break;
        }

        case SYSTEM_EVENT_STA_GOT_IP: {
            ESP_LOGI(TAG, "got ip:%s",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            xTaskCreate(check_ota, "ota", 8192, NULL, 5, NULL);
            break;
        }

        case SYSTEM_EVENT_AP_STACONNECTED: {
            ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                    MAC2STR(event->event_info.sta_connected.mac),
                    event->event_info.sta_connected.aid);
            break;
        }

        case SYSTEM_EVENT_AP_STADISCONNECTED: {
            ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                    MAC2STR(event->event_info.sta_disconnected.mac),
                    event->event_info.sta_disconnected.aid);
            break;
        }

        case SYSTEM_EVENT_STA_DISCONNECTED: {
            ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
            if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
                /*Switch to 802.11 bgn mode */
                esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
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

void
wifi_init_sta() {
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    nvs_handle nvs_wifi;
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    size_t ssid_len = sizeof(wifi_config.sta.ssid);
    size_t password_len = sizeof(wifi_config.sta.password);

    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READONLY, &nvs_wifi));
    ESP_ERROR_CHECK(nvs_get_str(nvs_wifi, "ssid", (char*)wifi_config.sta.ssid, &ssid_len));
    ESP_ERROR_CHECK(nvs_get_str(nvs_wifi, "password", (char*)wifi_config.sta.password, &password_len));
    nvs_close(nvs_wifi);

    ESP_LOGI(TAG, "wifi ssid (len=%d) '%s'", ssid_len, wifi_config.sta.ssid);
    ESP_LOGI(TAG, "wifi password (len=%d) '%s'", password_len, wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
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

    button_init();

    ssd1306_init(SSD1306_I2C, 4, 5);

    wifi_init_sta();

    /*ESP_LOGI(TAG, "waiting for IP");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    ESP_LOGI(TAG, "got IP, checking OTA");
    check_ota(); 

    ESP_LOGI(TAG, "OTA check done, no news"); */

    /*for (int i = 10; i >= 0; i--) {
        ESP_LOGD(TAG, "Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart(); */
}


#include "ssd1306.h"
#include "qrcodegen.h"
#include "font6x8.h"
#include "dns_server.h"

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

static const char *TAG = "simple wifi";

extern const unsigned char ota_ca_start[] asm("_binary_ota_ca_der_start");
extern const unsigned char ota_ca_end[] asm("_binary_ota_ca_der_end");

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;
const int GPS_GOT_FIX_BIT = BIT1;

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

// ------------------------------------------------------------------------------


#if 0
static int
tcp_connect(const char *host, int port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_IP,
        .ai_flags = AI_ADDRCONFIG | AI_V4MAPPED,
    };
    struct addrinfo *addrinfo;

    if (getaddrinfo(host, NULL, &hints, &addrinfo)) {
        return -1;
    }

    int fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket (family %d socktype %d protocol %d)", addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
        freeaddrinfo(addrinfo);
        return -1;
    }

    struct sockaddr_in *addr_ptr = (struct sockaddr_in *)addrinfo->ai_addr;
    addr_ptr->sin_port = htons(port);

    /* struct timeval tv = {
        .tv_sec = timeout_ms / 1000;
        .tv_usec = (timeout_ms % 1000) * 1000;
    };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); */

    /*int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK); */

    if (connect(fd, (const struct sockaddr*)addr_ptr, addrinfo->ai_addrlen)) {
        ESP_LOGE(TAG, "Failed to connnect to host (errno %d)", errno);
        close(fd);
        freeaddrinfo(addrinfo);
        return -1;
    }

    freeaddrinfo(addrinfo);
    return fd;
}
#endif

static const char OTA_SERVER[] = "ota.wodeewa.com";
static const char REQ_GET_OTA[] = "GET /index.html HTTP/1.1\r\nHost: ota.wodeewa.com\r\nUser-Agent: esp-idf/1.0 esp8266\r\n\r\n";

static void
my_debug(void *cfg __attribute__((unused)), int level, const char *file, int line, const char *str) {
    ESP_LOGI(TAG, "%d @ %s:%04d: %s", level, file, line, str);
}

static void
https_get_task(void *pvParameters) {
    char buf[512];
    int ret, len;

    while(1) {
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        /******************************************************************************
         * HTTPS conn start
         */

        mbedtls_net_context ssl_ctx;
        mbedtls_net_init(&ssl_ctx);

        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);

        mbedtls_ssl_config conf;
        mbedtls_ssl_config_init(&conf);
        mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);

        mbedtls_x509_crt cacert;
        mbedtls_x509_crt_init(&cacert);

        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_ctr_drbg_init(&ctr_drbg);

        mbedtls_entropy_context entropy;
        mbedtls_entropy_init(&entropy);

        time_t now;
        time(&now);
        ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)&now, sizeof(time_t));
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
            goto exit;
        }

        ret = mbedtls_x509_crt_parse_der(&cacert, ota_ca_start, ota_ca_end - ota_ca_start);
        if (ret < 0) {
            ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret);
            goto exit;
        }

        ret = mbedtls_net_connect(&ssl_ctx, OTA_SERVER, "443", MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_net_connect returned %d", ret);
            goto exit;
        }

        ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
            goto exit;
        }

        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
        /* A few notes on this MBEDTLS_SSL_VERIFY_OPTIONAL:
         *
         * 1. We don't have enough RAM to load a full-fledge CA bundle
         * 2. mbedtls doesn't support loading-verifying-freeing CA certs on-demand
         * 3. So we can have only 1-2 CA certs
         * 4. We don't have a precise time, so cert validity would fail with MBEDTLS_X509_BADCERT_FUTURE
         *
         * Therefore we set it to OPTIONAL do disable the automatic check-or-fail logic, and call mbedtls_ssl_get_verify_result() later manually
         */
        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
      	mbedtls_ssl_conf_cert_profile (&conf, &mbedtls_x509_crt_profile_next);

        ret = mbedtls_ssl_setup(&ssl, &conf);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", ret);
            goto exit;
        }

        ret = mbedtls_ssl_set_hostname(&ssl, OTA_SERVER);
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned %d", ret);
            goto exit;
        }

        mbedtls_ssl_set_bio(&ssl, &ssl_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

        while (1) {
            ret = mbedtls_ssl_handshake(&ssl);
            if (ret == 0) {
                break;
            }
            if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
                ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
                goto exit;
            }
        }

        ret = mbedtls_ssl_get_verify_result(&ssl);
        if (!(xEventGroupGetBits(wifi_event_group) & GPS_GOT_FIX_BIT)) {
            // Until we get a GPS fix, we don't know the time, so we can't check cert expiry.
            // Either we reject all expired certs or we accept all of them.
            // For the sake of being able to do OTA without GPS, *HERE* we accept them,
            // but from security perspective this is a *BAD THING*.
            ret &= ~MBEDTLS_X509_BADCERT_FUTURE;
        }
        if (ret != 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_get_verify_result returned 0x%x", ret);
            goto exit;
        }

        const char *wr_data = REQ_GET_OTA;
        ssize_t wr_remaining = sizeof(REQ_GET_OTA);
        while (wr_remaining > 0) {
            ret = mbedtls_ssl_write(&ssl, (const unsigned char*)wr_data, wr_remaining);
            if (ret >= 0) {
                wr_data += ret;
                wr_remaining -= ret;
            }
            else if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
                ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", ret);
                goto exit;
            }
        }

        while (1) {
            len = sizeof(buf) - 1;
            ret = mbedtls_ssl_read(&ssl, (unsigned char*)buf, len);

            if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE)) {
                continue;
            }

            if ((ret == 0) || (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)) {
                break;
            }

            if (ret < 0) {
                ESP_LOGE(TAG, "mbedtls_ssl_read returned %d", ret);
                break;
            }

            buf[ret] = '\0';
            mbedtls_printf("Read %d bytes\n%s", ret, buf);
        }

        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_net_free(&ssl_ctx);
        mbedtls_x509_crt_free(&cacert);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);

        ESP_LOGI(TAG, "Starting again!");
    }
}


void
app_main()
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP8266 chip with %d CPU cores, WiFi, silicon revision %d, %dMB %s flash\n",
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

    xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL);
    /*for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart(); */
}


#include "main.h"
#include "admin_mode.h"
#include "oled_stdout.h"
#include "dns_server.h"
#include "misc.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_libc.h>
#include <esp_event.h>
#include <esp_event_loop.h>
#include <esp_wifi.h>
#include <esp_http_server.h>

#include <stdio.h>
#include <string.h>

static const char *TAG = "admin";

static const char AP_SSID[] = "yadda";
static const char AP_PASSWORD[] = "yaddaboo";


/******************************************************************************
 * Show some info on the display
 */

void
display_wifi_conn(void) {
    // Encoding wifi parameters on QR-Code: https://github.com/zxing/zxing/wiki/Barcode-Contents#wi-fi-network-config-android-ios-11
    uint8_t buf[64];
    size_t len = snprintf((char*)buf, sizeof(buf), "WIFI:S:%s;T:WPA;P:%s;;", AP_SSID, AP_PASSWORD);

    lcd_clear();
    lcd_puts(11, 0, "SSID:");
    lcd_puts(11, 1, AP_SSID);
    lcd_puts(11, 3, "Password:");
    lcd_puts(11, 4, AP_PASSWORD);
    lcd_qr(buf, len);
}


void
display_portal_url(void) {
    uint8_t buf[64];
    char str_ip[16];

    tcpip_adapter_ip_info_t ap_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info);
    ip4addr_ntoa_r(&ap_info.ip, str_ip, sizeof(str_ip));

    // Encoding URLs on QR-Code: https://github.com/zxing/zxing/wiki/Barcode-Contents#url
    size_t len = snprintf((char*)buf, sizeof(buf), "http://%s/", str_ip);

    lcd_clear();
    lcd_puts(11, 0, "http://");
    lcd_puts(11, 1, str_ip);
    lcd_qr(buf, len);
}


/******************************************************************************
 * DNS Server details
 */

static bool
dns_policy(dns_buf_t *out, const char *name, dns_type_t type, dns_class_t _class, uint32_t *ttl) {
    ESP_LOGD(TAG, "dns query; type=%d, class=%d, name='%s'", type, _class, name);
    if (_class != DNS_CLASS_IN)
        return false;

    switch (type) {
        case DNS_TYPE_A: {
            // name is like "www.google.com." (note the trailing dot)
            *ttl = 180;
            dns_write_u32be(out, 0x0a000001); // rdata
            return true;
        }

        case DNS_TYPE_PTR: {
            // name is like "192.168.1.1.in-addr.arpa." (note the trailing dot)
            *ttl = 180;
            dns_write_dns_name(out, "www.yadda.boo"); // rdata
            return true;
        }

        default:
            return false;
    }
}


/******************************************************************************
 * HTTPS Server details
 */

static esp_err_t
http_generate_204_handler(httpd_req_t *req) {
    ESP_LOGD(TAG, "/generate_204");
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


static esp_err_t
root_get_handler(httpd_req_t *req) {
    ESP_LOGD(TAG, "root_get_handler;");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>Hello Secure World!</h1>", -1); // -1 = use strlen()
    return ESP_OK;
}


static const
httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};


static const
httpd_uri_t http_generate_204 = {
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = http_generate_204_handler
};



httpd_handle_t http_server = NULL;


/*******************************************************************************
 * Event handlers
 */


static esp_err_t
event_handler(void *ctx, system_event_t *event) {
    system_event_info_t *info = &event->event_info;
    
    switch (event->event_id) {
        case SYSTEM_EVENT_AP_START: {
            ESP_LOGI(TAG, "AP_START");
            tcpip_adapter_ip_info_t ap_info;
            char str_ip[16], str_netmask[16], str_gw[16];

            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info);
            ip4addr_ntoa_r(&ap_info.ip, str_ip, sizeof(str_ip));
            ip4addr_ntoa_r(&ap_info.netmask, str_netmask, sizeof(str_netmask));
            ip4addr_ntoa_r(&ap_info.gw, str_gw, sizeof(str_gw));
            ESP_LOGI(TAG, "AP started; ip=%s, netmask=%s, gw=%s", str_ip, str_netmask, str_gw);

            if (http_server == NULL) {
                ESP_LOGI(TAG, "Starting http server;");
                httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
                conf.max_uri_handlers = 8;
                conf.max_resp_headers = 8;
                esp_err_t ret = httpd_start(&http_server, &conf);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Error starting server; status=%d", ret);
                }
                else {
                    httpd_register_uri_handler(http_server, &root);
                    httpd_register_uri_handler(http_server, &http_generate_204);
                    ESP_LOGI(TAG, "Started http server;");
                }
            }

            dns_server_start(dns_policy);
            display_wifi_conn();
            break;
        }

        case SYSTEM_EVENT_AP_STACONNECTED: {
            ESP_LOGI(TAG, "AP_STACONNECTED station:" MACSTR " join, AID=%d", MAC2STR(info->sta_connected.mac), info->sta_connected.aid);
            display_portal_url();
            break;
        }

        case SYSTEM_EVENT_AP_STAIPASSIGNED: {
            ESP_LOGI(TAG, "AP_STAIPASSIGNED");
            break;
        }

        case SYSTEM_EVENT_AP_STADISCONNECTED: {
            ESP_LOGI(TAG, "AP_STADISCONNECTED station:" MACSTR "leave, AID=%d", MAC2STR(info->sta_disconnected.mac), info->sta_disconnected.aid);
            display_wifi_conn();
            break;
        }

        default:
            ESP_LOGI(TAG, "Unhandled event 0x%x", event->event_id);
            break;
    }
    return ESP_OK;
}

static void
wifi_init_ap(void) {
    ESP_ERROR_CHECK(esp_wifi_restore());
    esp_event_loop_set_cb(event_handler, NULL);

    tcpip_adapter_ip_info_t ap_info = {
        .ip =       { .addr = htonl(0x0a000001) },  // 10.0.0.1
        .netmask =  { .addr = htonl(0xff000000) },  // 255.0.0.0
        .gw =       { .addr = htonl(0x0a000001) },  // 10.0.0.1
    };
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    wifi_config_t wifi_config = { 0 };

    memcpy(wifi_config.ap.ssid, AP_SSID, sizeof(AP_SSID));
    wifi_config.ap.ssid_len = sizeof(AP_SSID) - 1;
    memcpy(wifi_config.ap.password, AP_PASSWORD, sizeof(AP_PASSWORD));
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_ap finished.");
}

static void
wifi_stop(void) {
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    esp_event_loop_set_cb(NULL, NULL);
    ESP_ERROR_CHECK(esp_wifi_stop());
}


void
admin_mode(void * pvParameters __attribute__((unused))) {
    wifi_init_ap();
    xEventGroupWaitBits(wifi_event_group, OTA_CHECK_DONE_BIT, false, true, portMAX_DELAY);
    wifi_stop();
    vTaskDelete(NULL);
}

// vim: set sw=4 ts=4 indk= et si:

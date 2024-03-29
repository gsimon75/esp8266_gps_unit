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
#include <esp_base64.h>
#include <nvs.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define HTTPD_411 "411 Length Required"
#define HTTPD_413 "413 Payload Too Large"
#define HTTPD_415 "415 Unsupported Media Type"

static const char *TAG = "admin";

extern const unsigned char admin_app_start[] asm("_binary_index_html_gz_start");
extern const unsigned char admin_app_end[] asm("_binary_index_html_gz_end");

static char AP_SSID[9];
static char AP_PASSWORD[9];

static int num_scanned_aps = 0;
static int allocated_scanned_aps = 0;
static char **scanned_aps = NULL;

/******************************************************************************
 * Misc helpers
 */

static void
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


static void
display_portal_url(void) {
    uint8_t buf[64];
    char str_ip[16];

    tcpip_adapter_ip_info_t ap_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info);
    ip4addr_ntoa_r(&ap_info.ip, str_ip, sizeof(str_ip));

    // Encoding URLs on QR-Code: https://github.com/zxing/zxing/wiki/Barcode-Contents#url
    size_t len = snprintf((char*)buf, sizeof(buf), "http://%s/app", str_ip);

    lcd_clear();
    lcd_puts(11, 0, "http://");
    lcd_puts(11, 1, str_ip);
    lcd_puts(11, 2, "/app");
    lcd_qr(buf, len);
}


static void
start_scan(void) {
    static const wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 0,
                .max = 0,
            },
        },
    };

    esp_err_t res = esp_wifi_scan_start(&scan_cfg, false);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %d", res);
    }
    else {
        ESP_LOGI(TAG, "Started WiFi scan");
    }
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
 * HTTPS Server helpers
 */

// NOTE: nvs blob size limit is 2k, so it makes no sense to struggle for handling more...
#define BUF_SIZE 2049
static httpd_handle_t http_server = NULL;


static void
log_req(const httpd_req_t *req) {
    ESP_LOGI(TAG, "%s %s (+ %u bytes)", http_method_str(req->method), req->uri, req->content_len);
}


static esp_err_t
httpd_resp_empty(httpd_req_t *req, const char *status) {
    httpd_resp_set_status(req, status);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


static bool
httpd_check_content_type(httpd_req_t *req, const char *shouldbe) {
    char ctype[64];
    ctype[0] = '\0';
    int res = httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype));
    res = (res == ESP_OK) && !strcmp(shouldbe, ctype);
    if (!res) {
        ESP_LOGW(TAG, "Content type mismatch, is='%s', shouldbe='%s'", ctype, shouldbe);
    }
    return res;
}


static bool
httpd_read_short_body(httpd_req_t *req, char* buf, size_t buflen) {
    if (!buf || !buflen) {
        ESP_LOGE(TAG, "Buffer does not exists");
        httpd_resp_empty(req, HTTPD_500);
        return false;
    }
    if (!req->content_len) {
        httpd_resp_empty(req, HTTPD_411);
        return false;
    }
    if (req->content_len >= buflen) { // not a bug: >= to reserve one byte for trailing zero
        httpd_resp_empty(req, HTTPD_413);
        return false;
    }

    char *p = buf;
    int remaining = req->content_len;
    while (remaining > 0) {
        int res = httpd_req_recv(req, p, remaining);
        if (res <= 0) {
            ESP_LOGE(TAG, "Failed to read request body: %d", res);
            return false;
        }
        p += res;
        remaining -= res;
    }
    *p = '\0';
    return true;
}

static bool
skip_white(const unsigned char **p) {
    while (isspace(**p)) ++*p;
    return **p;
}

// get an attribute of a one-level json object
// ugly as hell, but i don't want a full json parser with all the dynamic allocations
static bool
get_body_field(const unsigned char *p, const char *fieldname, char *dest, size_t destlen) {
    // sorry for the dense style, i don't want to stretch this junk to fifteen pages
    if (!skip_white(&p)) return false;
    if (*(p++) != '{') return false;

    while (1) {
        // find the beginning of the key
        if (!skip_white(&p)) return false;
        if (*(p++) != '"') return false;
        if (*p == '"') return false;
        const unsigned char *key = p++;

        // find the end of the key
        while (*p && (*p != '"')) ++p;
        if (!*p) return false;
        int keylen = p - key;

        // find the beginning of the value
        ++p;
        if (!skip_white(&p)) return false;
        if (*(p++) != ':') return false;
        if (!skip_white(&p)) return false;
        const unsigned char *value = p++;

        // find the end of the value
        if (*p == '"') {
            ++p;
            while (*p) {
                if (*p == '"') {
                    ++p;
                    break;
                }
                else if (*p == '\\') {
                    // supported: \t, \r, \n, \\, \", \xnn, the rest should go as utf8 bytestream
                    if (p[1] == 'x') {
                        if (!p[2] || !p[3]) return false;
                        p += 4;
                        continue;
                    }
                    p += 2;
                    continue;
                }
                ++p;
            }
        }
        else {
            while (*p && !isspace(*p) && (*p != ',') && (*p != '}')) ++p;
        }
        size_t valuelen = p - value;

        // check if we are looking for this key-value pair or not
        if ((!strncmp(fieldname, key, keylen) && !fieldname[keylen])) {
            --destlen; // reserve space for the trailing zero
            if (valuelen > destlen) valuelen = destlen;
            memcpy(dest, value, valuelen);
            dest[valuelen] = '\0';
            return true;
        }

        // find comma or closing curly
        if (!skip_white(&p)) return false;
        if (*p == '}') return false; // end of object
        ++p;
    }
}

static bool
decode_json_string(char *src) {
    if (!src || (*src != '"')) {
        return false;
    }
    char *end = src + strlen(src) - 1;
    if (*end != '"') {
        return false;
    }

    char hexbuf[3], *err;
    hexbuf[2] = '\0';

    *end = '\0';
    char *dst = src++;
    while (src < end) {
        char *backslash = strchr(src, '\\');
        if (!backslash) {
            if (src != dst) {
                size_t len = end - src;
                memcpy(dst, src, len);
                dst[len] = '\0';
            }
            return true;
        }
        if (src != dst) {
            size_t len = backslash - src;
            memcpy(dst, src, len);
            dst += len;
        }
        switch (backslash[1]) {
            case 'x': {
                hexbuf[0] = backslash[2];
                hexbuf[1] = backslash[3];
                *(dst++) = strtol(hexbuf, &err, 16);
                if (*err) {
                    return false;
                }
                src = backslash + 4;
                break;
            }
            case 't': {
                *(dst++) = '\t';
                src = backslash + 2;
                break;
            }
            case 'r': {
                *(dst++) = '\r';
                src = backslash + 2;
                break;
            }
            case 'n': {
                *(dst++) = '\n';
                src = backslash + 2;
                break;
            }
            default: {
                *(dst++) = backslash[1];
                src = backslash + 2;
                break;
            }
        }
    }
    *dst = '\0';
    return true;
}


static esp_err_t
nvs_str_from_attr(httpd_req_t *req, const char *attr, const char *nvs_part, const char *nvs_key) {
    char body[128], value[128];
    if (!httpd_read_short_body(req, body, sizeof(body))) {
        return ESP_OK;
    }
    ESP_LOGD(TAG, "Request body '%s'", body);
    if (!get_body_field(body, attr, value, sizeof(value))
        || !decode_json_string(value)
        ) {
        return httpd_resp_empty(req, HTTPD_400);
    }

    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READWRITE, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    res = nvs_set_str(nvs, nvs_key, value);
    nvs_close(nvs);

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot write persistent config, key='%s', error=%d", nvs_key, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    ESP_LOGI(TAG, "nvs.%s.%s='%s'", nvs_part, nvs_key, value);
    return httpd_resp_empty(req, HTTPD_204);
}


static esp_err_t
nvs_u16_from_attr(httpd_req_t *req, const char *attr, const char *nvs_part, const char *nvs_key) {

    char body[65], value[32];
    if (!httpd_read_short_body(req, body, sizeof(body))) {
        return ESP_OK;
    }
    ESP_LOGD(TAG, "Request body '%s'", body);
    if (!get_body_field(body, attr, value, sizeof(value))) {
        return httpd_resp_empty(req, HTTPD_400);
    }

    char *end = value;
    long x = strtol(value, &end, 0);
    if (*end || (x < 0) || (65535 < x)) {
        ESP_LOGE(TAG, "Invalid u16, name='%s', value='%s'", attr, value);
        return httpd_resp_empty(req, HTTPD_400);
    }

    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READWRITE, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    res = nvs_set_u16(nvs, nvs_key, x);
    nvs_close(nvs);

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot write persistent config, key='%s', error=%d", nvs_key, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    ESP_LOGI(TAG, "nvs.%s.%s=%lu=0x%lx", nvs_part, nvs_key, x, x);
    // yes, I know printf("nvs.%1$s.%2$s=%3$u=0x%3$x", "alpha", "bravo", 42), but it costs more confusion than the performance it spares
    return httpd_resp_empty(req, HTTPD_204);
}


static esp_err_t
nvs_blob_from_body(httpd_req_t *req, const char *mime_type, const char *nvs_part, const char *nvs_key) {
    char *body = (char*)malloc(BUF_SIZE);
    if (!body) {
        ESP_LOGE(TAG, "Out of memory");
        return httpd_resp_empty(req, HTTPD_500);
    }
    if (!httpd_read_short_body(req, body, BUF_SIZE)) {
        free(body);
        return ESP_OK;
    }
    if (!httpd_check_content_type(req, mime_type)) {
        return httpd_resp_empty(req, HTTPD_415);
    }

    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READWRITE, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    res = nvs_set_blob(nvs, nvs_key, body, req->content_len);
    nvs_close(nvs);

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot write persistent config, key='%s', error=%d", nvs_key, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    ESP_LOGI(TAG, "nvs.%s.%s=(%u bytes)", nvs_part, nvs_key, req->content_len);
    //hexdump(body, req->content_len);
    free(body);
    return httpd_resp_empty(req, HTTPD_204);
}


static esp_err_t
attr_from_nvs_str(httpd_req_t *req, const char *attr, const char *nvs_part, const char *nvs_key) {
    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READONLY, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    size_t value_len;
    res = nvs_get_str(nvs, nvs_key, NULL, &value_len);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read persistent config, key='%s', error=%d", nvs_key, res);
        nvs_close(nvs);
        return httpd_resp_empty(req, HTTPD_500);
    }
    size_t attrlen = strlen(attr);
    // {"...":"..."}\0
    size_t resplen = 2 + attrlen + 3 + value_len + 3;
    char *resp = (char *)malloc(1 + resplen);
    if (!resp) {
        ESP_LOGE(TAG, "Out of memory");
        return httpd_resp_empty(req, HTTPD_500);
    }

    char *p = resp;
    *(p++) = '{';

    *(p++) = '"';
    p = stpcpy(p, attr);
    *(p++) = '"';

    *(p++) = ':';

    *(p++) = '"';
    nvs_get_str(nvs, nvs_key, p, &value_len);
    nvs_close(nvs);
    p += value_len - 1;
    *(p++) = '"';

    *(p++) = '}';
    *p = '\0';
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, p - resp);
    free(resp);
    return ESP_OK;
}


static esp_err_t
attr_from_nvs_u16(httpd_req_t *req, const char *attr, const char *nvs_part, const char *nvs_key) {
    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READONLY, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    uint16_t value;
    res = nvs_get_u16(nvs, nvs_key, &value);
    nvs_close(nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read persistent config, key='%s', error=%d", nvs_key, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    size_t attrlen = strlen(attr);
    size_t resplen = 2 + attrlen + 2 + 5 + 2;
    char *resp = (char *)malloc(1 + resplen);
    if (!resp) {
        ESP_LOGE(TAG, "Out of memory");
        return httpd_resp_empty(req, HTTPD_500);
    }

    char *p = resp;
    *(p++) = '{';

    *(p++) = '"';
    p = stpcpy(p, attr);
    *(p++) = '"';

    *(p++) = ':';

    p += sprintf(p, "%u", value);

    *(p++) = '}';
    *p = '\0';
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, p - resp);
    free(resp);
    return ESP_OK;
}


static esp_err_t
body_from_nvs_blob(httpd_req_t *req, const char *mime_type, const char *nvs_part, const char *nvs_key) {
    nvs_handle nvs;
    esp_err_t res = nvs_open(nvs_part, NVS_READONLY, &nvs);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot find persistent config, part='%s', error=%d", nvs_part, res);
        return httpd_resp_empty(req, HTTPD_500);
    }
    size_t content_len = BUF_SIZE;
    char *body = (char*)malloc(content_len);
    if (!body) {
        ESP_LOGE(TAG, "Out of memory");
        nvs_close(nvs);
        return httpd_resp_empty(req, HTTPD_500);
    }
    res = nvs_get_blob(nvs, nvs_key, body, &content_len);
    nvs_close(nvs);

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot read persistent config, key='%s', error=%d", nvs_key, res);
        free(body);
        return httpd_resp_empty(req, HTTPD_500);
    }
    httpd_resp_set_type(req, mime_type);
    httpd_resp_send(req, body, content_len);
    free(body);
    return ESP_OK;
}


/******************************************************************************
 * HTTPS Server methods
 */

// ------------------------------------------------------------------------------
static esp_err_t
http_get_app(httpd_req_t *req) {
    log_req(req);
    char enc[16];

    int res = httpd_req_get_hdr_value_str(req, "Accept-Encoding", enc, sizeof(enc));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Client browser does not support gzip encoding");
        httpd_resp_set_status(req, "412 Need gzip support");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, admin_app_start, admin_app_end - admin_app_start);
    return ESP_OK;
}

static const
httpd_uri_t uri_get_app = {
    .uri       = "/app",
    .method    = HTTP_GET,
    .handler   = http_get_app,
};

// ------------------------------------------------------------------------------
static esp_err_t
http_get_generate_204(httpd_req_t *req) {
    log_req(req);
    return httpd_resp_empty(req, HTTPD_204);
}

static const
httpd_uri_t uri_get_generate_204 = {
    .uri       = "/generate_204",
    .method    = HTTP_GET,
    .handler   = http_get_generate_204,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_reboot(httpd_req_t *req) {
    log_req(req);
    TimerHandle_t timer = xTimerCreate("reboot", pdMS_TO_TICKS(1000), pdFALSE, NULL, (TimerCallbackFunction_t)esp_restart);
    if (!timer) {
        ESP_LOGE(TAG, "Failed to create reboot timer");
        return httpd_resp_empty(req, HTTPD_500);
    }
    xTimerStart(timer, 0);
    return httpd_resp_empty(req, HTTPD_204);
}

static const
httpd_uri_t uri_post_reboot = {
    .uri       = "/rest/reboot",
    .method    = HTTP_POST,
    .handler   = http_post_reboot,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_wifi_ssids(httpd_req_t *req) {
    log_req(req);
    size_t resplen = 1; // closing square at the end
    for (int i = 0; i < num_scanned_aps; ++i) {
        resplen += 1 + 1 + strlen(scanned_aps[i]) + 1; // opening square or comma, quote, string, quote
    }
    char *resp = (char *)malloc(1 + resplen);
    if (!resp) {
        ESP_LOGE(TAG, "Out of memory");
        return httpd_resp_empty(req, HTTPD_500);
    }
    char *p = resp;
    for (int i = 0; i < num_scanned_aps; ++i) {
        *(p++) = i ? ',' : '[';
        *(p++) = '"';
        p = stpcpy(p, scanned_aps[i]);
        *(p++) = '"';
    }
    *(p++) = ']';
    *p = '\0';
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, p - resp);
    free(resp);
    start_scan(); // start a new scan, so the user has a way to refresh the info
    return ESP_OK;
}

static const
httpd_uri_t uri_get_wifi_ssids = {
    .uri       = "/rest/wifi/ssids",
    .method    = HTTP_GET,
    .handler   = http_get_wifi_ssids,
};

// ------------------------------------------------------------------------------
static esp_err_t
http_get_wifi_ssid(httpd_req_t *req) {
    log_req(req);
    return attr_from_nvs_str(req, "ssid", "wifi", "ssid");
}

static const
httpd_uri_t uri_get_wifi_ssid = {
    .uri       = "/rest/wifi/ssid",
    .method    = HTTP_GET,
    .handler   = http_get_wifi_ssid,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_wifi_ssid(httpd_req_t *req) {
    log_req(req);
    // D (54662) admin: Request body '{"ssid":"charlie"}'
    return nvs_str_from_attr(req, "ssid", "wifi", "ssid");
}

static const
httpd_uri_t uri_post_wifi_ssid = {
    .uri       = "/rest/wifi/ssid",
    .method    = HTTP_POST,
    .handler   = http_post_wifi_ssid,
};


// ------------------------------------------------------------------------------

// NOTE: Intentionally not exporting wifi password


// ------------------------------------------------------------------------------
static esp_err_t
http_post_wifi_password(httpd_req_t *req) {
    log_req(req);
    // D (108984) admin: Request body '{"password":"qwer"}'
    return nvs_str_from_attr(req, "password", "wifi", "password");
}

static const
httpd_uri_t uri_post_wifi_password = {
    .uri       = "/rest/wifi/password",
    .method    = HTTP_POST,
    .handler   = http_post_wifi_password,
};


// ------------------------------------------------------------------------------

// NOTE: Intentionally not exporting private key


// ------------------------------------------------------------------------------
static esp_err_t
http_post_ssl_pkey(httpd_req_t *req) {
    log_req(req);
    return nvs_blob_from_body(req, "application/pkcs8", "ssl", "pkey");
}

static const
httpd_uri_t uri_post_ssl_pkey = {
    .uri       = "/rest/ssl/pkey",
    .method    = HTTP_POST,
    .handler   = http_post_ssl_pkey,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_ssl_cert(httpd_req_t *req) {
    log_req(req);
    return body_from_nvs_blob(req, "application/x-x509-user-cert", "ssl", "cert");
}

static const
httpd_uri_t uri_get_ssl_cert = {
    .uri       = "/rest/ssl/cert",
    .method    = HTTP_GET,
    .handler   = http_get_ssl_cert,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_ssl_cert(httpd_req_t *req) {
    log_req(req);
    return nvs_blob_from_body(req, "application/x-x509-user-cert", "ssl", "cert");
}

static const
httpd_uri_t uri_post_ssl_cert = {
    .uri       = "/rest/ssl/cert",
    .method    = HTTP_POST,
    .handler   = http_post_ssl_cert,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_ssl_cacert(httpd_req_t *req) {
    log_req(req);
    return body_from_nvs_blob(req, "application/x-x509-ca-cert", "ssl", "cacert");
}

static const
httpd_uri_t uri_get_ssl_cacert = {
    .uri       = "/rest/ssl/cacert",
    .method    = HTTP_GET,
    .handler   = http_get_ssl_cacert,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_ssl_cacert(httpd_req_t *req) {
    log_req(req);
    return nvs_blob_from_body(req, "application/x-x509-ca-cert", "ssl", "cacert");
}

static const
httpd_uri_t uri_post_ssl_cacert = {
    .uri       = "/rest/ssl/cacert",
    .method    = HTTP_POST,
    .handler   = http_post_ssl_cacert,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_ota_url(httpd_req_t *req) {
    log_req(req);
    return attr_from_nvs_str(req, "ota_url", "ota", "url");
}

static const
httpd_uri_t uri_get_ota_url = {
    .uri       = "/rest/ota/url",
    .method    = HTTP_GET,
    .handler   = http_get_ota_url,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_ota_url(httpd_req_t *req) {
    log_req(req);
    // D (289555) admin: Request body '{"ota_url":"https://ota.wodeewa.com/out"}'
    return nvs_str_from_attr(req, "ota_url", "ota", "url");
}

static const
httpd_uri_t uri_post_ota_url = {
    .uri       = "/rest/ota/url",
    .method    = HTTP_POST,
    .handler   = http_post_ota_url,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_server_url(httpd_req_t *req) {
    log_req(req);
    return attr_from_nvs_str(req, "data_url", "server", "url");
}

static const
httpd_uri_t uri_get_server_url = {
    .uri       = "/rest/server/url",
    .method    = HTTP_GET,
    .handler   = http_get_server_url,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_server_url(httpd_req_t *req) {
    log_req(req);
    // D (369076) admin: Request body '{"data_url":"https://alpha.wodeewa.com/gps-reports"}'
    return nvs_str_from_attr(req, "data_url", "server", "url");
}

static const
httpd_uri_t uri_post_server_url = {
    .uri       = "/rest/server/url",
    .method    = HTTP_POST,
    .handler   = http_post_server_url,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_server_time_threshold(httpd_req_t *req) {
    log_req(req);
    return attr_from_nvs_u16(req, "time_threshold", "server", "time_trshld");
}

static const
httpd_uri_t uri_get_server_time_threshold = {
    .uri       = "/rest/server/time_threshold",
    .method    = HTTP_GET,
    .handler   = http_get_server_time_threshold,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_server_time_threshold(httpd_req_t *req) {
    log_req(req);
    // D (318487) admin: Request body '{"time_threshold":30}'
    return nvs_u16_from_attr(req, "time_threshold", "server", "time_trshld");
}

static const
httpd_uri_t uri_post_server_time_threshold = {
    .uri       = "/rest/server/time_threshold",
    .method    = HTTP_POST,
    .handler   = http_post_server_time_threshold,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_get_server_distance_threshold(httpd_req_t *req) {
    log_req(req);
    return attr_from_nvs_u16(req, "distance_threshold", "server", "dist_trshld");
}

static const
httpd_uri_t uri_get_server_distance_threshold = {
    .uri       = "/rest/server/distance_threshold",
    .method    = HTTP_GET,
    .handler   = http_get_server_distance_threshold,
};


// ------------------------------------------------------------------------------
static esp_err_t
http_post_server_distance_threshold(httpd_req_t *req) {
    log_req(req);
    // D (345347) admin: Request body '{"distance_threshold":100}'
    return nvs_u16_from_attr(req, "distance_threshold", "server", "dist_trshld");
}

static const
httpd_uri_t uri_post_server_distance_threshold = {
    .uri       = "/rest/server/distance_threshold",
    .method    = HTTP_POST,
    .handler   = http_post_server_distance_threshold,
};


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
                conf.max_open_sockets = 15;
                conf.max_uri_handlers = 24;
                conf.max_resp_headers = 8;
                esp_err_t res = httpd_start(&http_server, &conf);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "Error starting server; status=%d", res);
                }
                else {
                    httpd_register_uri_handler(http_server, &uri_get_app);
                    httpd_register_uri_handler(http_server, &uri_get_generate_204);
                    httpd_register_uri_handler(http_server, &uri_post_reboot);
                    httpd_register_uri_handler(http_server, &uri_get_wifi_ssids);

                    httpd_register_uri_handler(http_server, &uri_get_wifi_ssid);
                    httpd_register_uri_handler(http_server, &uri_post_wifi_ssid);

                    // NOTE: Intentionally not exporting wifi password
                    httpd_register_uri_handler(http_server, &uri_post_wifi_password);

                    // NOTE: Intentionally not exporting private key
                    httpd_register_uri_handler(http_server, &uri_post_ssl_pkey);

                    httpd_register_uri_handler(http_server, &uri_get_ssl_cert);
                    httpd_register_uri_handler(http_server, &uri_post_ssl_cert);
                    
                    httpd_register_uri_handler(http_server, &uri_get_ssl_cacert);
                    httpd_register_uri_handler(http_server, &uri_post_ssl_cacert);

                    httpd_register_uri_handler(http_server, &uri_get_ota_url);
                    httpd_register_uri_handler(http_server, &uri_post_ota_url);

                    httpd_register_uri_handler(http_server, &uri_get_server_url);
                    httpd_register_uri_handler(http_server, &uri_post_server_url);
                    
                    httpd_register_uri_handler(http_server, &uri_get_server_time_threshold );
                    httpd_register_uri_handler(http_server, &uri_post_server_time_threshold );
                    
                    httpd_register_uri_handler(http_server, &uri_get_server_distance_threshold );
                    httpd_register_uri_handler(http_server, &uri_post_server_distance_threshold );
                    ESP_LOGI(TAG, "Started http server;");
                }
            }

            dns_server_start(dns_policy);
            display_wifi_conn();
            start_scan();
            break;
        }

        case SYSTEM_EVENT_SCAN_DONE: {
            esp_err_t res;
            ESP_LOGD(TAG, "SCAN_DONE");
            ESP_LOGD(TAG, "SCAN_DONE status=%d, number=%d, scan_id=%d", info->scan_done.status, info->scan_done.number, info->scan_done.scan_id);

            uint16_t num_aps = 0;
            res = esp_wifi_scan_get_ap_num(&num_aps);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "get_ap_num failed: %d", res);
                break;
            }
            if (num_aps == 0) {
                ESP_LOGW(TAG, "Zero APs found");
                break;
            }
            ESP_LOGD(TAG, "Scan ok, num_aps=%d", num_aps);
            wifi_ap_record_t *aps = (wifi_ap_record_t*)malloc(num_aps * sizeof(wifi_ap_record_t));
            if (!aps) {
                ESP_LOGW(TAG, "Out of memory");
                break;
            }

            res = esp_wifi_scan_get_ap_records(&num_aps, aps);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "Scan failed: %d", res);
                break;
            }
            ESP_LOGD(TAG, "APs retrieved, num_aps=%d", num_aps);
            num_scanned_aps = 0;
            if (allocated_scanned_aps < num_aps) {
                scanned_aps = (char**)realloc(scanned_aps, num_aps * sizeof(char*));
                if (!scanned_aps) {
                    ESP_LOGW(TAG, "Out of memory");
                    break;
                }
                for (int i = allocated_scanned_aps; i < num_aps; ++i) {
                    scanned_aps[i] = (char*)malloc(32);
                    if (!scanned_aps[i]) {
                        ESP_LOGW(TAG, "Out of memory");
                        break;
                    }
                }
                allocated_scanned_aps = num_aps;
            }
            else if (num_aps < allocated_scanned_aps) {
                for (int i = num_aps; i < allocated_scanned_aps; ++i) {
                    free(scanned_aps[i]);
                    scanned_aps[i] = NULL;
                }
                scanned_aps = (char**)realloc(scanned_aps, num_aps * sizeof(char*));
                if (!scanned_aps) {
                    ESP_LOGW(TAG, "Out of memory");
                    break;
                }
                allocated_scanned_aps = num_aps;
            }
            for (int i = 0; i < num_aps; ++i) {
                ESP_LOGD(TAG, "AP %d: ssid='%s', channel=%d, bssid=%02x:%02x:%02x:%02x:%02x:%02x",
                    i, aps[i].ssid, aps[i].primary, aps[i].bssid[0], aps[i].bssid[1], aps[i].bssid[2], aps[i].bssid[3], aps[i].bssid[4], aps[i].bssid[5]);
                memcpy(scanned_aps[i], aps[i].ssid, 32);
                scanned_aps[i][32 - 1] = '\0'; // asciiz ensured
            }
            free(aps);
            aps = NULL;
            num_scanned_aps = num_aps;
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


static bool
wifi_init_ap(void) {
    esp_err_t res = esp_wifi_restore();
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Wifi restore failed: %d", res);
    }

    esp_event_loop_set_cb(event_handler, NULL);

    tcpip_adapter_ip_info_t ap_info = {
        .ip =       { .addr = htonl(0x0a000001) },  // 10.0.0.1
        .netmask =  { .addr = htonl(0xff000000) },  // 255.0.0.0
        .gw =       { .addr = htonl(0x0a000001) },  // 10.0.0.1
    };

    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    res = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ap_info);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Could not set AP ip info");
        printf("AP setup failed\n");
        return false;
    }
    res = tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Could not start dhcps");
        printf("DHCPs start failed\n");
        return false;
    }

    wifi_config_t wifi_config = { 0 };

    uint32_t rndbuf[2];

    rndbuf[0] = esp_random();
    rndbuf[1] = esp_random();
    esp_base64_encode(rndbuf, 6, AP_SSID, sizeof(AP_SSID));
    rndbuf[0] = esp_random();
    rndbuf[1] = esp_random();
    esp_base64_encode(rndbuf, 6, AP_PASSWORD, sizeof(AP_PASSWORD));

    ESP_LOGI(TAG, "Ephemeral SSID=%s, password=%s", AP_SSID, AP_PASSWORD);

    strncpy(wifi_config.ap.ssid, AP_SSID, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(AP_SSID);
    strncpy(wifi_config.ap.password, AP_PASSWORD, sizeof(wifi_config.ap.password));
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    //res = esp_wifi_set_mode(WIFI_MODE_AP);
    res = esp_wifi_set_mode(WIFI_MODE_APSTA); // for scanning as well
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot set WiFi AP mode: %d", res);
        printf("AP mode error\n");
        return false;
    }

    res = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot configure WiFi AP: %d", res);
        printf("AP config failed\n");
        return false;
    }

    res = esp_wifi_start();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Cannot start WiFi AP: %d", res);
        printf("AP start failed\n");
        return false;
    }

    ESP_LOGI(TAG, "AP started");
    return true;
}


/*static void
wifi_stop(void) {
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    esp_event_loop_set_cb(NULL, NULL);
    ESP_ERROR_CHECK(esp_wifi_stop());
}*/


esp_err_t
admin_mode_start(void) {
    wifi_init_ap();
    ESP_LOGD(TAG, "Started");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <rom/ets_sys.h>

#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_event_loop.h>
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

#include <esp_tls.h>
#include <esp_ota_ops.h>

#include <nvs_flash.h>

#include <string.h>
#include <ctype.h>

#define ustrchr(s, c)   ((unsigned char*)strchr((const char*)(s), c))

static const char *TAG = "ota";

extern const unsigned char ota_ca_start[] asm("_binary_ota_ca_der_start");
extern const unsigned char ota_ca_end[] asm("_binary_ota_ca_der_end");

extern const unsigned char client_pkey_start[] asm("_binary_client_key_der_start");
extern const unsigned char client_pkey_end[] asm("_binary_client_key_der_end");

extern const unsigned char client_cert_start[] asm("_binary_client_crt_der_start");
extern const unsigned char client_cert_end[] asm("_binary_client_crt_der_end");

/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event - are we connected to the AP with an IP? */
#define WIFI_CONNECTED_BIT  BIT0
#define GPS_GOT_FIX_BIT  BIT1

static const char OTA_SERVER[] = "ota.wodeewa.com";
static const char OTA_PATH[] = "/out/test";

/* NOTE: nginx has a stupid default interpretation of If-Modified-Since, don't forget to override it:
 * http://nginx.org/en/docs/http/ngx_http_core_module.html#if_modified_since
 * https://trac.nginx.org/nginx/ticket/93
 */

ssize_t
read_some(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len) {
    while (1) {
        ssize_t ret = mbedtls_ssl_read(ssl, buf, len);

        if ((ret == MBEDTLS_ERR_SSL_WANT_READ) || (ret == MBEDTLS_ERR_SSL_WANT_WRITE)) {
            continue;
        }

        if ((ret == 0) || (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)) {
            return 0; // EOF
        }

        if (ret < 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned %d", ret);
            return ret;
        }
        return ret;
    }
}

typedef enum {
    STATE_STATUSLINE,
    STATE_HEADER,
    STATE_BODY,
    STATE_DONE,
    STATE_ERROR
} http_read_state_t;


void
hexdump(const unsigned char *data, ssize_t len) {
    size_t offs = 0;
    char linebuf[4 + 2 + 3*16 + 1 + 16 + 1];
    while (len > 0) {
        int i;
        char *p = linebuf;
        p += sprintf(p, "%04x:", offs);
        for (i = 0; (i < len) && (i < 0x10); ++i) {
            p += sprintf(p, " %02x", data[i]);
        }
        for (; i < 0x10; ++i) {
            *(p++) = ' ';
            *(p++) = ' ';
            *(p++) = ' ';
        }
        *(p++) = ' ';
        for (i = 0; (i < len) && (i < 0x10); ++i) {
            *(p++) = isprint(data[i]) ? data[i] : '.'; 
        }
        ESP_LOGD(TAG, "%s", linebuf);
        offs += 0x10;
        len -= 0x10;
    }
}



#define LOG_PARTITION(name, p) do { \
    ESP_LOGI(TAG, name " partition: address=0x%08x, size=0x%08x, type=%d, subtype=%d, label='%p'", (p)->address, (p)->size, (p)->type, (p)->subtype, (p)->label); \
    hexdump((p)->label, 14); \
    } while (0)



#define BUFSIZE 48
void
check_ota(void * pvParameters __attribute__((unused))) {
    unsigned char buf[BUFSIZE + 1];
    int ret;
    char req_get_ota[192];

    ESP_LOGI(TAG, "Checking OTA");

    // get current partition, its label is expected to be "fw_<hex timestamp>"
    const esp_partition_t *running = esp_ota_get_running_partition();
    LOG_PARTITION("Running", running);

    // extract time
    time_t ts;
    if (strncmp(running->label, "fw_", 3)) {
        ESP_LOGW(TAG, "Current partition name is not recognized");
        ts = 0;
    }
    else {
        char *end;
        ts = strtol(running->label + 3, &end, 16);
        if (*end) {
            ESP_LOGW(TAG, "Current partition name/timestamp is not valid");
            ts = 0;
        }
    }
    char sts[32];
    strftime(sts, sizeof(sts), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&ts));
    ESP_LOGD(TAG, "Current part timestamp: '%s'", sts);

    // format the http request
    snprintf(req_get_ota, sizeof(req_get_ota), "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp8266\r\nIf-Modified-Since: %s\r\n\r\n", OTA_PATH, OTA_SERVER, sts);
    ESP_LOGD(TAG, "Request: '%s'", req_get_ota);

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    LOG_PARTITION("Update", update);

    /******************************************************************************
     * HTTPS conn start
     */

    mbedtls_net_context ssl_ctx;
    mbedtls_net_init(&ssl_ctx);

    mbedtls_ssl_context ssl;
    mbedtls_ssl_init(&ssl);

    mbedtls_ssl_config conf;
    mbedtls_ssl_config_init(&conf);

    mbedtls_x509_crt cacert;
    mbedtls_x509_crt_init(&cacert);

    mbedtls_x509_crt client_cert;
    mbedtls_x509_crt_init(&client_cert);

    mbedtls_pk_context client_pkey;
    mbedtls_pk_init(&client_pkey);

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

    ret = mbedtls_x509_crt_parse_der(&client_cert, client_cert_start, client_cert_end - client_cert_start);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret);
        goto exit;
    }

    ret = mbedtls_pk_parse_key(&client_pkey, client_pkey_start, client_pkey_end - client_pkey_start, NULL, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key returned -0x%x", -ret);
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
     * 1. We don't have enough RAM to load a full-fledge CA bundle
     * 2. mbedtls doesn't support loading-verifying-freeing CA certs on-demand
     * 3. So we can have only 1-2 CA certs
     * 4. We don't have a precise time, so cert validity would fail with MBEDTLS_X509_BADCERT_FUTURE
     * Therefore we set it to OPTIONAL do disable the automatic check-or-fail logic, and call mbedtls_ssl_get_verify_result() later manually
     */
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_cert_profile(&conf, &mbedtls_x509_crt_profile_next);
    mbedtls_ssl_conf_own_cert(&conf, &client_cert, &client_pkey);

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

    const char *wr_data = req_get_ota;
    ssize_t wr_remaining = sizeof(req_get_ota);
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

    unsigned char *start = buf;
    http_read_state_t state = STATE_STATUSLINE;
    size_t content_remaining = 0;
    while ((state < STATE_DONE) && ((start - buf) < BUFSIZE)) { // exit if buffer is too short for a line
        ssize_t ret = read_some(&ssl, start, BUFSIZE - (start - buf));
        if (ret <= 0) {
            break;
        }
        start[ret] = '\0';
        //ESP_LOGD(TAG, "Read %d bytes:'%s'\n", ret, buf);

        if (state < STATE_BODY) {
            start[ret] = '\0';
            ret += start - buf;
            start = buf;
            while (ret > 0) {
                unsigned char *eol = ustrchr(start, '\n');
                if (!eol) { // unfinished line
                    break;
                }
                eol[(eol[-1] == '\r') ? -1 : 0] = '\0'; // (cr)lf to nul

                unsigned char* header_line = start;

                ret -= (eol + 1 - start);
                start = eol + 1;

                if (!header_line[0]) { // end of header
                    state = STATE_BODY;
                    break;
                }
                // HERE: @header_line points to a header line
                if (state == STATE_STATUSLINE) {
                    unsigned char *sep = ustrchr(header_line, ' ');
                    if (!sep) { // invalid response status line
                        ESP_LOGE(TAG, "Invalid response status '%s'", header_line);
                        state = STATE_ERROR;
                        break;
                    }
                    int response_status = atoi((const char*)(sep + 1));
                    if (response_status == 200) {
                        state = STATE_HEADER;
                    }
                    else if (response_status == 304) {
                        ESP_LOGI(TAG, "No newer OTA is available");
                        state = STATE_DONE;
                    }
                    else if (response_status < 400) {
                        state = STATE_DONE;
                    }
                    else {
                        ESP_LOGE(TAG, "Response error: '%d'", response_status);
                        state = STATE_ERROR;
                    }
                }
                else {
                    unsigned char *sep = ustrchr(header_line, ':');
                    if (!sep) { // invalid response header line
                        ESP_LOGE(TAG, "Invalid response header '%s'", header_line);
                        state = STATE_ERROR;
                        break;
                    }
                    for (*(sep++) = '\0'; *sep && ((*sep == ' ') || (*sep == '\t')); ++sep)
                        ;

                    // HERE: header_line=key, sep=value
                    // ESP_LOGD(TAG, "Header line; key='%s', value='%s'", header_line, sep);
                    if (!strcasecmp("Content-Length", header_line)) {
                        content_remaining = atoi((char*)sep);
                        ESP_LOGI(TAG, "OTA length: %u", content_remaining);
                    }
                }
            }

            if (ret == 0) {
                start = buf;
                ret = 0;
            }
            else {
                memmove(buf, start, ret);
                start = buf + ret;
            }
        }

        if (state == STATE_BODY) {
            if (ret > content_remaining) {
                ret = content_remaining;
            }
            start[ret] = '\0';

            // HERE: data=start len=ret
            ESP_LOGD(TAG, "Body (len=%d): '%s'", ret, start);

            start = buf;
            content_remaining -= ret;
            if (content_remaining == 0) {
                state = STATE_DONE;
            }
        }
    }

    mbedtls_ssl_close_notify(&ssl);

exit:
    mbedtls_net_free(&ssl_ctx);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    ESP_LOGI(TAG, "OTA check done");
    vTaskDelete(NULL);
}



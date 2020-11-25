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
static const char OTA_FIRMWARE_PATH[] = "/out/firmware.desc";
#define BUFSIZE 512


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
        *(p++) = '\0';
        ESP_LOGD(TAG, "%s", linebuf);
        offs += 0x10;
        data += 0x10;
        len -= 0x10;
    }
}

#define LOG_PARTITION(name, p) do { \
    ESP_LOGI(TAG, name " partition: address=0x%08x, size=0x%08x, type=%d, subtype=%d, label='%p'", (p)->address, (p)->size, (p)->type, (p)->subtype, (p)->label); \
    hexdump((p)->label, 14); \
    } while (0)


uint32_t
fletcher32(const uint8_t *data, size_t count)
{
   uint32_t sum1 = 0, sum2 = 0;
   for (size_t index = 0; index < count; ++index) {
      sum1 = (sum1 + data[index] + (((uint32_t)data[index + 1]) << 8)) % 65535;
      sum2 = (sum2 + sum1) % 65535;
   }
   return (sum2 << 16) | sum1;
}


typedef struct {
    mbedtls_net_context ssl_ctx;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_pkey;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    unsigned char buf[BUFSIZE + 1];
    unsigned char *rdpos, *wrpos;
    size_t content_length, content_remaining;
    bool error;
} https_conn_context_t;


void
https_conn_destroy(https_conn_context_t *ctx) {
    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_net_free(&ctx->ssl_ctx);
    mbedtls_x509_crt_free(&ctx->cacert);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
}


void
https_conn_init(https_conn_context_t *ctx) {
    ctx->rdpos = ctx->wrpos = ctx->buf;
    ctx->content_length = 0;
    ctx->content_remaining = 0;

    mbedtls_net_init(&ctx->ssl_ctx);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_x509_crt_init(&ctx->client_cert);
    mbedtls_pk_init(&ctx->client_pkey);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);

    int ret;
    time_t now;
    
    time(&now);
    ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy, (const unsigned char *)&now, sizeof(time_t));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        goto exit;
    }

    ret = mbedtls_x509_crt_parse_der(&ctx->cacert, ota_ca_start, ota_ca_end - ota_ca_start);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret);
        goto exit;
    }

    ret = mbedtls_x509_crt_parse_der(&ctx->client_cert, client_cert_start, client_cert_end - client_cert_start);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret);
        goto exit;
    }

    ret = mbedtls_pk_parse_key(&ctx->client_pkey, client_pkey_start, client_pkey_end - client_pkey_start, NULL, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_pk_parse_key returned -0x%x", -ret);
        goto exit;
    }

    ret = mbedtls_net_connect(&ctx->ssl_ctx, OTA_SERVER, "443", MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect returned %d", ret);
        goto exit;
    }

    ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    /* A few notes on this MBEDTLS_SSL_VERIFY_OPTIONAL:
     * 1. We don't have enough RAM to load a full-fledge CA bundle
     * 2. mbedtls doesn't support loading-verifying-freeing CA certs on-demand
     * 3. So we can have only 1-2 CA certs
     * 4. We don't have a precise time, so cert validity would fail with MBEDTLS_X509_BADCERT_FUTURE
     * Therefore we set it to OPTIONAL do disable the automatic check-or-fail logic, and call mbedtls_ssl_get_verify_result() later manually
     */
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    mbedtls_ssl_conf_cert_profile(&ctx->conf, &mbedtls_x509_crt_profile_next);
    mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->client_cert, &ctx->client_pkey);

    ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", ret);
        goto exit;
    }

    ret = mbedtls_ssl_set_hostname(&ctx->ssl, OTA_SERVER);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned %d", ret);
        goto exit;
    }

    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->ssl_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

    while (1) {
        ret = mbedtls_ssl_handshake(&ctx->ssl);
        if (ret == 0) {
            break;
        }
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
            goto exit;
        }
    }

    ret = mbedtls_ssl_get_verify_result(&ctx->ssl);
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

    ctx->error = false;
    return true;

exit:
    ctx->error = true;
    https_conn_destroy(ctx);
    return false;
}


bool
send_buf(https_conn_context_t *ctx) {
    while (ctx->rdpos < ctx->wrpos) {
        ssize_t ret = mbedtls_ssl_write(&ctx->ssl, ctx->rdpos, ctx->wrpos - ctx->rdpos);
        if (ret >= 0) {
            ctx->rdpos += ret;
        }
        else if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", ret);
            ctx->error = true;
            return false;
        }
    }
    ctx->error = false;
    ctx->rdpos = ctx->wrpos = ctx->buf;
    return true;
}


ssize_t
read_some(https_conn_context_t *ctx) {
    while (1) {
        ssize_t ret = mbedtls_ssl_read(&ctx->ssl, ctx->wrpos, ctx->buf + BUFSIZE - ctx->wrpos);

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


bool
send_GET_request(https_conn_context_t *ctx, const char *file_path) {
    ctx->rdpos = ctx->buf;
    ctx->wrpos = ctx->buf + snprintf(ctx->buf, BUFSIZE, "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/1.0 esp8266\r\n\r\n", file_path, OTA_SERVER);
    ctx->content_length = 0;
    ctx->content_remaining = 0xffffffff; // no known read limit on header length
    ESP_LOGD(TAG, "Request: '%s'", ctx->buf);
    return send_buf(ctx);

}


unsigned char *
http_readline(https_conn_context_t *ctx) {
    unsigned char *eol;

    unsigned char *
    terminate_line(void) { // helper: terminate a line, move rdpos over it, and return its start
        eol[((ctx->buf < eol) && (eol[-1] == '\r')) ? -1 : 0] = '\0'; // replace (cr)lf with nul
        unsigned char *result = ctx->rdpos;
        ctx->content_remaining -= eol + 1 - ctx->rdpos;
        ctx->rdpos = eol + 1;
        return result;
    }

    if (ctx->rdpos != ctx->wrpos) { // there is unread data in the buffer
        eol = (unsigned char*)memchr(ctx->rdpos, '\n', ctx->wrpos - ctx->rdpos);
        if (eol) { // it contains a terminated line
            return terminate_line();
        }
        // no eol in the buffer, we'll need to read some more data, so let's make space for that
        if (ctx->rdpos != ctx->buf) {
            memmove(ctx->buf, ctx->rdpos, ctx->wrpos - ctx->rdpos);
            ctx->wrpos -= (ctx->rdpos - ctx->buf);
            ctx->rdpos = ctx->buf;
        }
    }
    else { // buffer is empty, so make it empty at the start position
        ctx->rdpos = ctx->wrpos = ctx->buf;
    }

    while (ctx->wrpos < (ctx->buf + BUFSIZE)) {
        ssize_t ret = read_some(ctx);
        if (ret <= 0) { // either error or eof before eol
            ctx->error = true;
            return NULL;
        }
        eol = (unsigned char*)memchr(ctx->wrpos, '\n', ret); // search only in the data we read now
        ctx->wrpos += ret; // move wrpos over this chunk
        if (eol) {
            return terminate_line();
        }
    }
    ctx->error = true;
    return NULL; // haven't returned yet -> buffer was too short for a line
}


int
read_http_statusline(https_conn_context_t *ctx) {
    unsigned char *line = http_readline(ctx);
    if (!line) { // read error, too long line, etc.
        return -1;
    }
    unsigned char *sep = ustrchr(line, ' ');
    if (!sep) { // invalid response status line
        ctx->error = true;
        return -1;
    }
    return atoi((const char*)(sep + 1));
}


bool
read_http_header(https_conn_context_t *ctx, unsigned char **name, unsigned char **value) {
    if (ctx->content_remaining == 0) {
        return 0;
    }
    unsigned char *line = http_readline(ctx);
    if (!line) { // read error, too long line, etc.
        return false;
    }
    if (!*line) { // empty line -> end of headers
        ctx->content_remaining = ctx->content_length;
        return false;
    }
    unsigned char *sep = ustrchr(line, ':');
    if (!sep) { // malformed header line
        ctx->error = true;
        return false;
    }
    for (*(sep++) = '\0'; *sep && ((*sep == ' ') || (*sep == '\t')); ++sep) {
    }
    *name = line;
    *value = sep;
    if (!strcasecmp("Content-Length", line)) {
        ctx->content_length = atoi((char*)sep);
    }
    return true;
}


bool
read_http_body(https_conn_context_t *ctx, unsigned char **data, size_t *datalen) {
    if (ctx->content_remaining == 0) {
        return false;
    }
    if (ctx->rdpos < ctx->wrpos) {
        *data = ctx->rdpos;
        *datalen = ctx->wrpos - ctx->rdpos;
        if (*datalen > ctx->content_remaining) {
            *datalen = ctx->content_remaining;
        }
        ctx->content_remaining -= *datalen;
        ctx->rdpos = ctx->wrpos = ctx->buf;
        return true;
    }

    ssize_t ret = read_some(ctx);
    if (ret <= 0) { // eof or error
        ctx->error |= (ret < 0);
        return false;
    }
 
    *data = ctx->wrpos;
    *datalen = ret;
    ctx->content_remaining -= ret;
    return true;
}


void
check_ota(void * pvParameters __attribute__((unused))) {
    ESP_LOGI(TAG, "Checking OTA");

    // get current partition, its label is expected to be "fw_<hex timestamp>"
    const esp_partition_t *running = esp_ota_get_running_partition();
    LOG_PARTITION("Running", running);

    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    LOG_PARTITION("Update", update);

    https_conn_context_t ctx;

    ESP_LOGI(TAG, "Connecting to OTA server");
    https_conn_init(&ctx);

    // https content-related data

    int status;
    unsigned char *name, *value;
    unsigned char *data;
    size_t datalen;

    char fw_path[32];
    fw_path[0] = '\0';
    uint32_t fw_checksum = 0;
    time_t fw_timestamp = 0;

    // get the descriptor file first

    ESP_LOGI(TAG, "Getting OTA descriptor");
    if (!send_GET_request(&ctx, OTA_FIRMWARE_PATH)) {
        goto exit;
    }

    status = read_http_statusline(&ctx);
    ESP_LOGD(TAG, "Response status: '%d'", status);
    if (status != 200) {
        goto exit;
    }

    while (read_http_header(&ctx, &name, &value)) {
        ESP_LOGD(TAG, "Header line; name='%s', value='%s'", name, value);
    }
    ESP_LOGI(TAG, "OTA descriptor length: %u", ctx.content_length);

    while (read_http_header(&ctx, &name, &value)) {
        ESP_LOGD(TAG, "Body line; name='%s', value='%s'", name, value);
        if (!strcmp(name, "path")) {
            strncpy(fw_path, value, sizeof(fw_path));
        }
        else if (!strcmp(name, "mtime")) {
            fw_timestamp = strtol((const char*)value, NULL, 0);
        }
        else if (!strcmp(name, "fletcher32")) {
            fw_checksum = strtol((const char*)value, NULL, 0);
        }
    }
    ESP_LOGI(TAG, "OTA descriptor end");

    // get the firmware binary

    if (fw_path[0]) {
        ESP_LOGI(TAG, "Getting OTA binary '%s'", fw_path);
        if (!send_GET_request(&ctx, fw_path)) {
            goto exit;
        }

        int status = read_http_statusline(&ctx);
        ESP_LOGD(TAG, "Response status: '%d'", status);
        if (status != 200) {
            goto exit;
        }

        unsigned char *name, *value;
        while (read_http_header(&ctx, &name, &value)) {
            ESP_LOGD(TAG, "Header line; name='%s', value='%s'", name, value);
        }
        ESP_LOGI(TAG, "OTA binary length: %u", ctx.content_length);

        while (read_http_body(&ctx, &data, &datalen)) {
            ESP_LOGD(TAG, "Body chunk; len=%d, remaining=%u", datalen, ctx.content_remaining);
            //hexdump(data, datalen);
        }
        ESP_LOGI(TAG, "OTA binary end");
    }

exit:
    https_conn_destroy(&ctx);

    ESP_LOGI(TAG, "OTA check done");
    vTaskDelete(NULL);
}



#include "main.h"
#include "misc.h"

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

#include <nvs.h>

#include <string.h>
#include <ctype.h>

static const char *TAG = "ota";

static char *OTA_SERVER_NAME, *OTA_SERVER_PORT, *OTA_PATH, *OTA_DESCRIPTOR_FILENAME;

#define BUFSIZE 512


#define LOG_PARTITION(name, p) ESP_LOGI(TAG, name " partition: address=0x%08x, size=0x%08x, type=%d, subtype=%d, label='%s'", (p)->address, (p)->size, (p)->type, (p)->subtype, (p)->label);


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


static void
https_conn_destroy(https_conn_context_t *ctx) {
    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_net_free(&ctx->ssl_ctx);
    mbedtls_x509_crt_free(&ctx->cacert);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
}


static bool
get_blob(nvs_handle h, const char *name, uint8_t **buf, size_t *buflen) {
    esp_err_t res = nvs_get_blob(h, name, NULL, buflen);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Cannot find blob '%s': %d", name, res);
        return false;
    }
    *buf = (uint8_t*)malloc(*buflen);
    if (!*buf) {
        ESP_LOGW(TAG, "Cannot allocate %u bytes for '%s'", *buflen, name);
        return false;
    }
    res = nvs_get_blob(h, name, *buf, buflen);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Cannot read blob '%s': %d", name, res);
        free(*buf);
        *buf = NULL;
        return false;
    }
    //ESP_LOGD(TAG, "Read blob '%s' of %u bytes", name, *buflen);
    //hexdump(*buf, *buflen);
    return true;
}


static bool
https_conn_init(https_conn_context_t *ctx) {
    ctx->rdpos = ctx->wrpos = ctx->buf;
    ctx->content_length = 0;
    ctx->content_remaining = 0;

    //ESP_LOGD(TAG, "Checkpt in %s %s:%d", __FUNCTION__, __FILE__, __LINE__);
    mbedtls_net_init(&ctx->ssl_ctx);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&ctx->conf, 4);
#endif // CONFIG_MBEDTLS_DEBUG
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_x509_crt_init(&ctx->client_cert);
    mbedtls_pk_init(&ctx->client_pkey);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);

    time_t last_time, now;
    
    esp_err_t res = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy, NULL, 0);
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", res);
        goto close_conn;
    }

    nvs_handle nvs;

    res = nvs_open("ssl", NVS_READONLY, &nvs);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "Cannot find persistent SSL config: %d", res);
        // proceed without client-side cert
    }
    else {
        uint8_t *blob = NULL;
        size_t bloblen = 0;

        if (get_blob(nvs, "cacert", &blob, &bloblen)) {
            res = mbedtls_x509_crt_parse_der(&ctx->cacert, blob, bloblen);
            if (res < 0) {
                ESP_LOGW(TAG, "Failed to parse cacert: -0x%x", -res);
            }
            free(blob);
            blob = NULL;
            bloblen = 0;
        }

        if (get_blob(nvs, "cert", &blob, &bloblen)) {
            res = mbedtls_x509_crt_parse_der(&ctx->client_cert, blob, bloblen);
            if (res < 0) {
                ESP_LOGW(TAG, "Failed to parse cert: -0x%x", -res);
            }
            free(blob);
            blob = NULL;
            bloblen = 0;
        }

        if (get_blob(nvs, "pkey", &blob, &bloblen)) {
            res = mbedtls_pk_parse_key(&ctx->client_pkey, blob, bloblen, NULL, 0);
            if (res < 0) {
                ESP_LOGW(TAG, "Failed to parse pkey: -0x%x", -res);
            }
            free(blob);
            blob = NULL;
            bloblen = 0;
        }
        nvs_close(nvs);
    }

    res = mbedtls_net_connect(&ctx->ssl_ctx, OTA_SERVER_NAME, OTA_SERVER_PORT, MBEDTLS_NET_PROTO_TCP);
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect returned %d", res);
        goto close_conn;
    }

    res = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", res);
        goto close_conn;
    }

    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    /* A few notes on this MBEDTLS_SSL_VERIFY_OPTIONAL:
     * 1. We don't have enough RAM to load a full-fledge CA bundle
     * 2. mbedtls doesn't support loading-verifying-freeing CA certs on-demand
     * 3. So we can have only 1-2 CA certs
     * 4. We don't have a precise time, so cert validity would fail with MBEDTLS_X509_BADCERT_FUTURE
     * Therefore we set it to OPTIONAL do disable the automatic check-or-fail logic, and call mbedtls_ssl_get_verify_result() later manually
     */
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL); // FIXME: if present
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    mbedtls_ssl_conf_cert_profile(&ctx->conf, &mbedtls_x509_crt_profile_next);
    mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->client_cert, &ctx->client_pkey); // FIXME: if present

    res = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned %d", res);
        goto close_conn;
    }

    res = mbedtls_ssl_set_hostname(&ctx->ssl, OTA_SERVER_NAME);
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned %d", res);
        goto close_conn;
    }

    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->ssl_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

    time(&last_time);
    while (1) {
        time(&now);
        if ((now - last_time) > 1) {
            ESP_LOGD(TAG, "Handshake in progress");
            last_time = now;
        }
        res = mbedtls_ssl_handshake(&ctx->ssl);
        if (res == 0) {
            break;
        }
        if ((res != MBEDTLS_ERR_SSL_WANT_READ) && (res != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -res);
            goto close_conn;
        }
    }

    res = mbedtls_ssl_get_verify_result(&ctx->ssl);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec < SOURCE_DATE_EPOCH) {
        // Until we get a GPS fix, we don't know the time, so we can't check cert expiry.
        // Either we reject all expired certs or we accept all of them.
        // For the sake of being able to do OTA without GPS, *HERE* we accept them,
        // but from security perspective this is a *BAD THING*.
        res &= ~MBEDTLS_X509_BADCERT_FUTURE;
    }
    if (res != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_get_verify_result returned 0x%x", res);
        goto close_conn;
    }

    ctx->error = false;
    return true;

close_conn:
    ctx->error = true;
    return false;
}


static bool
send_buf(https_conn_context_t *ctx) {
    while (ctx->rdpos < ctx->wrpos) {
        ssize_t res = mbedtls_ssl_write(&ctx->ssl, ctx->rdpos, ctx->wrpos - ctx->rdpos);
        if (res >= 0) {
            ctx->rdpos += res;
        }
        else if ((res != MBEDTLS_ERR_SSL_WANT_READ) && (res != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", res);
            ctx->error = true;
            return false;
        }
    }
    ctx->error = false;
    ctx->rdpos = ctx->wrpos = ctx->buf;
    return true;
}


static ssize_t
read_some(https_conn_context_t *ctx) {
    while (1) {
        ssize_t res = mbedtls_ssl_read(&ctx->ssl, ctx->wrpos, ctx->buf + BUFSIZE - ctx->wrpos);

        if ((res == MBEDTLS_ERR_SSL_WANT_READ) || (res == MBEDTLS_ERR_SSL_WANT_WRITE)) {
            continue;
        }

        if ((res == 0) || (res == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)) {
            return 0; // EOF
        }

        if (res < 0) {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned %d", res);
            return res;
        }
        return res;
    }
}


static bool
send_GET_request(https_conn_context_t *ctx, const char *server, const char *path, const char *file_name) {
    ESP_LOGD(TAG, "OTA_SERVER_NAME='%s'", OTA_SERVER_NAME);
    ESP_LOGD(TAG, "OTA_SERVER_PORT='%s'", OTA_SERVER_PORT);
    ESP_LOGD(TAG, "OTA_PATH='%s'", OTA_PATH);
    ESP_LOGD(TAG, "OTA_DESCRIPTOR_FILENAME='%s'", OTA_DESCRIPTOR_FILENAME);

    ctx->rdpos = ctx->buf;
    ctx->wrpos = ctx->buf + snprintf(ctx->buf, BUFSIZE, "GET %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/" STR(SOURCE_DATE_EPOCH) " esp8266\r\n\r\n", path, file_name, server);
    ctx->content_length = 0;
    ctx->content_remaining = 0xffffffff; // no known read limit on header length
    ESP_LOGD(TAG, "Request:\n%s", ctx->buf);
    return send_buf(ctx);

}


static unsigned char *
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
        ssize_t res = read_some(ctx);
        if (res <= 0) { // either error or eof before eol
            ctx->error = true;
            return NULL;
        }
        eol = (unsigned char*)memchr(ctx->wrpos, '\n', res); // search only in the data we read now
        ctx->wrpos += res; // move wrpos over this chunk
        if (eol) {
            return terminate_line();
        }
    }
    ctx->error = true;
    return NULL; // haven't returned yet -> buffer was too short for a line
}


static int
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


static bool
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


static bool
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

    ssize_t res = read_some(ctx);
    if (res <= 0) { // eof or error
        ctx->error |= (res < 0);
        return false;
    }
 
    *data = ctx->wrpos;
    *datalen = res;
    ctx->content_remaining -= res;
    return true;
}


void
check_ota(void * pvParameters __attribute__((unused))) {
    time_t last_time, now;
    esp_err_t res;

    ESP_LOGI(TAG, "Checking OTA");
    printf("Checking OTA...\n");

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

    if (strncmp("https://", url, 8)) {
        ESP_LOGE(TAG, "Won't accept firmware from insecure source");
        printf("OTA security error\n");
        goto close_conn;
    }

    // separate server name(:port) and path
    // NOTE: no new allocations, so only @url will have to be freed
    char *first_slash = strchr(url + 8, '/');
    if (first_slash) {
        // need a zero between the name and the path, so overwrite "https://", it won't be needed anyway
        size_t len = first_slash - (url + 8);
        memcpy(url, url + 8, len);
        url[len] = '\0';
        OTA_SERVER_NAME = url;

        // need a zero between the path and the filename as well, so move back the path too
        char *last_slash = strrchr(first_slash, '/');
        if (!last_slash[1]) {
            // just a path, no filename: no need to move anything
            OTA_PATH = first_slash;
            OTA_DESCRIPTOR_FILENAME = "";
        }
        else {
            OTA_PATH = url + len + 1;
            len = last_slash - first_slash + 1;
            memcpy(OTA_PATH, first_slash, len);
            OTA_PATH[len] = '\0';
            OTA_DESCRIPTOR_FILENAME = last_slash + 1;
        }
    }
    else {
        // no path: the part after "https://" is the server name
        OTA_SERVER_NAME = url + 8;
        OTA_PATH = "/";
        OTA_DESCRIPTOR_FILENAME = "";
    }

    // separate name and port
    OTA_SERVER_PORT = strchr(OTA_SERVER_NAME, ':'); // FIXME: no support for "user:pwd@hostname:port"
    if (OTA_SERVER_PORT) {
        *(OTA_SERVER_PORT++) = '\0';
    }
    else {
        OTA_SERVER_PORT = "443";
    }

    ESP_LOGI(TAG, "Connecting to OTA server");
    task_info();
    if (!https_conn_init(&ctx)) {
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
    if (!send_GET_request(&ctx, OTA_SERVER_NAME, OTA_PATH, fw_name)) {
        goto close_conn;
    }
#else
    if (!send_GET_request(&ctx, OTA_SERVER_NAME, OTA_PATH, OTA_DESCRIPTOR_FILENAME)) {
        goto close_conn;
    }
#endif // MULTI_IMAGES

    status = read_http_statusline(&ctx);
    if (status != 200) {
        ESP_LOGE(TAG, "Response error %d", status);
        goto close_conn;
    }

    while (read_http_header(&ctx, &name, &value)) {
        //ESP_LOGD(TAG, "Header line; name='%s', value='%s'", name, value);
    }
    //ESP_LOGD(TAG, "OTA descriptor length: %u", ctx.content_length);

    while (read_http_header(&ctx, &name, &value)) {
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

    if (fw_mtime <= SOURCE_DATE_EPOCH) {
        ESP_LOGI(TAG, "Current firmware is up-to-date");
        printf("Firmware up-to-date\n");
    }
    else if (fw_name[0]) { // get the firmware binary
        ESP_LOGI(TAG, "Current firmware: %u, available: %lu", SOURCE_DATE_EPOCH, fw_mtime);
        uint16_t sum1, sum2; // Fletcher16: don't want to deal with odd-bytes-long chunks

        ESP_LOGI(TAG, "Getting OTA binary '%s'", fw_name);
        if (!send_GET_request(&ctx, OTA_SERVER_NAME, OTA_PATH, fw_name)) {
            goto close_conn;
        }

        int status = read_http_statusline(&ctx);
        if (status != 200) {
            ESP_LOGE(TAG, "Response error %d", status);
            goto close_conn;
        }

        unsigned char *name, *value;
        while (read_http_header(&ctx, &name, &value)) {
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
        printf("Downloading firmware\n");
        while (read_http_body(&ctx, &data, &datalen)) {
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
            ssize_t page = i / BUFSIZE;
            if (page != current_page) {
                res = spi_flash_read(update->address + (page * BUFSIZE), ctx.buf, BUFSIZE);
                if (res != ESP_OK) {
                    ESP_LOGE(TAG, "flash read failed, error=0x%x", res);
                    goto close_conn;
                }
                current_page = page;
            }
            sum1 = (sum1 + ctx.buf[i % BUFSIZE]) % 255;
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
    ESP_LOGI(TAG, "OTA check done");
    task_info();

    if (url) {
        free(url);
        url = NULL;
    }
    https_conn_destroy(&ctx);

    xEventGroupSetBits(main_event_group, OTA_CHECK_DONE_BIT);
    vTaskDelete(NULL);
}



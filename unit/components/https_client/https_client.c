#include "https_client.h"
#include "misc.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <esp_log.h>

#include <nvs.h>

#include <string.h>
#include <ctype.h>
#include <stdarg.h>

static const char *TAG = "httpscli";

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


void
https_conn_destroy(https_conn_context_t *ctx) {
    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_net_free(&ctx->ssl_ctx);
    mbedtls_x509_crt_free(&ctx->client_cert);
    mbedtls_x509_crt_free(&ctx->cacert);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
}


bool
https_conn_init(https_conn_context_t *ctx, const char *server_name, const char *server_port) {
    ctx->rdpos = ctx->wrpos = ctx->buf;
    ctx->content_length = 0;
    ctx->content_remaining = 0;

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

    res = mbedtls_net_connect(&ctx->ssl_ctx, server_name, server_port, MBEDTLS_NET_PROTO_TCP);
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

    res = mbedtls_ssl_set_hostname(&ctx->ssl, server_name);
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
        ssize_t res = mbedtls_ssl_read(&ctx->ssl, ctx->wrpos, ctx->buf + HTTPS_CLIENT_BUFSIZE - ctx->wrpos);

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


bool
https_send_request(https_conn_context_t *ctx, const char *method, const char *server, const char *path, const char *resource, const char *extra_headers, ...) {
    if (!extra_headers) {
        extra_headers = "";
    }
    ctx->rdpos = ctx->wrpos = ctx->buf;
    ctx->wrpos += snprintf(ctx->wrpos, ctx->buf + HTTPS_CLIENT_BUFSIZE - ctx->wrpos, "%s %s%s HTTP/1.1\r\nHost: %s\r\nUser-Agent: esp-idf/" STR(SOURCE_DATE_EPOCH) " esp8266\r\n", method, path, resource, server);
    if (extra_headers) {
        va_list ap;
        va_start(ap, extra_headers);
        ctx->wrpos += vsnprintf(ctx->wrpos, ctx->buf + HTTPS_CLIENT_BUFSIZE - ctx->wrpos, extra_headers, ap);
        va_end(ap);
    }
    ctx->wrpos += snprintf(ctx->wrpos, ctx->buf + HTTPS_CLIENT_BUFSIZE - ctx->wrpos, "\r\n");
    ctx->content_length = 0;
    ctx->content_remaining = 0xffffffff; // no known read limit on header length
    //ESP_LOGD(TAG, "Request:\n%s", ctx->buf);
    return send_buf(ctx);
}


bool
https_send_data(https_conn_context_t *ctx, const uint8_t *data, size_t datalen) {
    while (datalen > 0) {
        ssize_t res = mbedtls_ssl_write(&ctx->ssl, data, datalen);
        if (res >= 0) {
            data += res;
            datalen -= res;
        }
        else if ((res != MBEDTLS_ERR_SSL_WANT_READ) && (res != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned %d", res);
            ctx->error = true;
            return false;
        }
    }
    return true;
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

    while (ctx->wrpos < (ctx->buf + HTTPS_CLIENT_BUFSIZE)) {
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


int
https_read_statusline(https_conn_context_t *ctx) {
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
https_read_header(https_conn_context_t *ctx, unsigned char **name, unsigned char **value) {
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
https_read_body_chunk(https_conn_context_t *ctx, unsigned char **data, size_t *datalen) {
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


bool
https_split_url(char *url, char **server_name, char **server_port, char **path, char **resource) {
    if (strncmp("https://", url, 8)) {
        return false;
    }

    // separate server name(:port) and path
    // NOTE: no new allocations, so only @url will have to be freed
    char *first_slash = strchr(url + 8, '/');
    if (first_slash) {
        // need a zero between the name and the path, so overwrite "https://", it won't be needed anyway
        size_t len = first_slash - (url + 8);
        memcpy(url, url + 8, len);
        url[len] = '\0';
        *server_name = url;

        // need a zero between the path and the filename as well, so move back the path too
        char *last_slash = strrchr(first_slash, '/');
        if (!last_slash[1]) {
            // just a path, no filename: no need to move anything
            *path = first_slash;
            *resource = "";
        }
        else {
            *path = url + len + 1;
            len = last_slash - first_slash + 1;
            memcpy(*path, first_slash, len);
            (*path)[len] = '\0';
            *resource = last_slash + 1;
        }
    }
    else {
        // no path: the part after "https://" is the server name
        *server_name = url + 8;
        *path = "/";
        *resource = "";
    }

    // separate name and port
    *server_port = strchr(*server_name, ':'); // FIXME: no support for "user:pwd@hostname:port"
    if (*server_port) {
        *((*server_port)++) = '\0';
    }
    else {
        *server_port = "443";
    }
    return true;
}

// vim: set sw=4 ts=4 indk= et si:

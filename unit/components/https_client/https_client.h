#ifndef HTTPS_CLIENT_H
#define HTTPS_CLIENT_H

#include <esp_tls.h>
#include <stdbool.h>
#include <stdarg.h>

#define HTTPS_CLIENT_BUFSIZE 512

typedef struct {
    mbedtls_net_context ssl_ctx;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_pkey;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    unsigned char buf[HTTPS_CLIENT_BUFSIZE + 1];
    unsigned char *rdpos, *wrpos;
    size_t content_length, content_remaining;
} https_conn_context_t;

bool https_init(https_conn_context_t *ctx);
bool https_connect(https_conn_context_t *ctx, const char *server_name, const char *server_port);
bool https_send_request(https_conn_context_t *ctx, const char *method, const char *server, const char *path, const char *resource, const char *extra_headers, ...);
bool https_send_data(https_conn_context_t *ctx, const uint8_t *data, size_t datalen);
int https_read_statusline(https_conn_context_t *ctx);
bool https_read_header(https_conn_context_t *ctx, unsigned char **name, unsigned char **value);
bool https_read_body_chunk(https_conn_context_t *ctx, unsigned char **data, size_t *datalen);
void https_disconnect(https_conn_context_t *ctx);
void https_destroy(https_conn_context_t *ctx);

// NOTE: changes the string pointed by @url, but the returned pointers will point into this area, so they needn't (and mustn't) be freed individually
bool https_split_url(char *url, char **server_name, char **server_port, char **path, char **resource);

#endif // HTTPS_CLIENT_H
// vim: set sw=4 ts=4 indk= et si:

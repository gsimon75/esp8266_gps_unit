#include "oled_stdout.h"
#include "ssd1306.h"
#include "font6x8.h"
#include "qrcodegen.h"

//#undef LOG_LOCAL_LEVEL
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <esp_log.h>
#include <esp_libc.h>
#include <stdio.h>
#include <string.h>

//#define INVERSE_FOR_QR

static const char *TAG = "oled_stdout";

static int the_port;
static int col, row;

#define MAXCOL 21
#define MAXROW 8

void
lcd_gotoxy(int ncol, int nrow) {
    col = ncol;
    row = nrow;
}

void
lcd_clear(void) {
    ssd1306_clear(the_port);
    ssd1306_send_cmd_byte(the_port, SSD1306_DISPLAY_NORMAL);
}

void
lcd_putchar(int col, int row, char c) {
    if ((c < 0x20) || (c & 0x80)) {
        c = 0x20;
    }
    col *= 6;
    ssd1306_set_range(the_port, col + 2, col + 2 + 5, row, row);
    ssd1306_send_data(the_port, &font6x8[6 * (c - 0x20)], 6);
}


void
lcd_puts(int col, int row, const char *s) {
    if (!*s) {
        return;
    }
    col *= 6;
    ssd1306_set_range(the_port, col + 2, 127, row, row);
    static uint8_t send_data_cmd[] = {
        0x78, 0x40,
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, send_data_cmd, sizeof(send_data_cmd), true);
    while (*s) {
        char c = *s;
        if ((c < 0x20) || (c & 0x80)) {
            i2c_master_write(cmd, (uint8_t*)&font6x8[0], 6, true);
        }
        else {
            i2c_master_write(cmd, (uint8_t*)&font6x8[6 * (c - 0x20)], 6, true);
        }
        ++s;
    }
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(the_port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
}


int
lcd_write(void *cookie, const char *buf, int n) {
    int orig_n = n;

    for (; n > 0; --n) {
        switch (*buf) {
            case '\t':
                for (; (col & 0x07) && (col < MAXCOL); ++col) {
                    lcd_putchar(col, row, ' ');
                }
                if (col < MAXCOL) {
                    break;
                }
                // intentional fallthrough
            case '\n':
            newline:
                row = (row + 1) % MAXROW;
                // intentional fallthrough
            case '\r':
                col = 0;
                break;
            default:
                if (col == 0) { // clear that row
                    int prev_row = (row + MAXROW - 1) % MAXROW;
                    ssd1306_set_range(the_port, 0, 0, prev_row, prev_row);
                    ssd1306_send_data_byte(the_port, 0);
                    ssd1306_set_range(the_port, 0, 127, row, row);
                    ssd1306_send_data_byte(the_port, 0x18);
                    ssd1306_memset(the_port, 0, 127);
                }
                lcd_putchar(col, row, *buf);
                ++col;
                if (col >= MAXCOL) {
                    goto newline;
                }
                break;
        }
        ++buf;
    }
    return orig_n;
}


// QR-Code versions, sizes and information capacity: https://www.qrcode.com/en/about/version.html
// Version 3 = 29 x 29, binary info cap by ecc mode: L:53, M:42, Q:32 H:24 bytes
// Version 11 = 61 x 61, binary info cap by ecc mode: L:321, M:251, Q:177, H:137 bytes
#define QR_VERSION 3
#define QR_MODE qrcodegen_Ecc_LOW
//#define QR_VERSION 11
//#define QR_MODE qrcodegen_Ecc_HIGH

#define QR_BUFSIZE (qrcodegen_BUFFER_LEN_FOR_VERSION(QR_VERSION))
#define QR_RAW_SIZE (17 + 4 * (QR_VERSION))

#if QR_RAW_SIZE <= 32
#   define QR_SIZE ((QR_RAW_SIZE) * 2)
#else
#   define QR_SIZE QR_RAW_SIZE
#endif // QR_RAW_SIZE <= 32

#define QR_OUTBUFSIZE ((QR_SIZE) * 8 + 1)
#define QR_OFFSET (32 - (QR_SIZE) / 2) 

esp_err_t
lcd_qr(const uint8_t *input, ssize_t input_length) {
    if (!input) {
        ESP_LOGE(TAG, "QR input is invalid");
        return ESP_FAIL;
    }
    if (input_length < 0) {
        input_length = strlen(input);
    }
    if (input_length > QR_BUFSIZE) {
        ESP_LOGE(TAG, "QR input is too long");
        return ESP_FAIL;
    }
    esp_err_t result = ESP_FAIL;
    uint8_t *tempBuffer = (uint8_t*)malloc(QR_OUTBUFSIZE);
    uint8_t *qrcode = (uint8_t*)malloc(QR_BUFSIZE);

    if (!tempBuffer || !qrcode) {
        goto done;
    }
    memcpy(tempBuffer, input, input_length);

    if (!qrcodegen_encodeBinary(tempBuffer, input_length, qrcode, QR_MODE, QR_VERSION, QR_VERSION, qrcodegen_Mask_AUTO, true)) {
        ESP_LOGE(TAG, "Failed to generate QR code");
        goto done;
    }
    uint8_t *p = tempBuffer;

    for (int page_y = 0; page_y < 64; page_y += 8) {
        for (int x = 0; x < QR_SIZE; ++x) {
            int pix8 = 0;
            for (int yb = 0; yb < 8; ++yb) {
                int y = page_y + yb - QR_OFFSET;
#if QR_RAW_SIZE <= 32
                if (qrcodegen_getModule(qrcode, x / 2, y / 2)) {
#else
                if (qrcodegen_getModule(qrcode, x, y)) {
#endif // QR_RAW_SIZE <= 32
                    pix8 |= 1 << yb;
                }
            }
#ifdef INVERSE_FOR_QR
            *(p++) = pix8;
#else // INVERSE_FOR_QR
            *(p++) = ~pix8;
#endif // INVERSE_FOR_QR
        }
    }
    ssd1306_set_range(the_port, QR_OFFSET, QR_OFFSET + QR_SIZE - 1, 0, 7);

    ssd1306_send_data(the_port, tempBuffer, p - tempBuffer);
#ifdef INVERSE_FOR_QR
    ssd1306_send_cmd_byte(the_port, SSD1306_DISPLAY_INVERSE);
#endif // INVERSE_FOR_QR
    ssd1306_set_range(the_port, 0, 127, 0, 7);
    result = ESP_OK;

done:
    if (qrcode) {
        free(qrcode);
    }
    if (tempBuffer) {
        free(tempBuffer);
    }
    return result;
}


esp_err_t
lcd_init(int port) {
    the_port = port;
    col = row = 0;
    FILE *f = fwopen(NULL, lcd_write);
    if (!f) {
        return ESP_FAIL;
    }
    setbuf(f, NULL);
    esp_log_set_putchar(ets_putc);
    fclose(stdout);
    stdout = f;
    ESP_LOGI(TAG, "stdout reassigned to oled display");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

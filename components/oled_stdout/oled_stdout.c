#include "oled_stdout.h"
#include "ssd1306.h"
#include "font6x8.h"

//#undef LOG_LOCAL_LEVEL
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <esp_log.h>
#include <esp_libc.h>
#include <stdio.h>


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
}

void
lcd_putchar(int col, int row, char c) {
    if ((c < 0x20) || (c & 0x80)) {
        c = 0x20;
    }
    col *= 6;
    ssd1306_set_range(the_port, col, col + 5, row, row);
    ssd1306_send_data(the_port, &font6x8[6 * (c - 0x20)], 6);
}


void
lcd_puts(int col, int row, const char *s) {
    if (!*s) {
        return;
    }
    col *= 6;
    ssd1306_set_range(the_port, col, 127, row, row);
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


esp_err_t
lcd_init(int port) {
    the_port = port;
    col = row = 0;
    FILE *f = fwopen(NULL, lcd_write);
    if (!f) {
        return ESP_FAIL;
    }
    esp_log_set_putchar(ets_putc);
    fclose(stdout);
    stdout = f;
    ESP_LOGI(TAG, "stdout reassigned to oled display");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

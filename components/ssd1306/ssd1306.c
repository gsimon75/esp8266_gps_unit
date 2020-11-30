#include "ssd1306.h"
#include <stdio.h>
#include <math.h>

#define TEST_PATTERNS 1

esp_err_t
ssd1306_send_cmd_byte(i2c_port_t port, uint8_t code) {
    uint8_t send_command_cmd[] = {
        0x78, 0x00, code
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, send_command_cmd, sizeof(send_command_cmd), true);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t
ssd1306_send_data_byte(i2c_port_t port, uint8_t value) {
    uint8_t send_data_cmd[] = {
        0x78, 0x40, value
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, send_data_cmd, sizeof(send_data_cmd), true);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t
ssd1306_send_data(i2c_port_t port, const uint8_t* data, uint16_t n) {
    static uint8_t send_data_cmd[] = {
        0x78, 0x40,
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, send_data_cmd, sizeof(send_data_cmd), true);
    i2c_master_write(cmd, (uint8_t*)data, n, true);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t
ssd1306_memset(i2c_port_t port, uint8_t value, uint16_t n) {
    static uint8_t send_data_cmd[] = {
        0x78, 0x40,
    };
    uint8_t value_unroll[] = {
        value, value, value, value, value, value, value, value, value, value, value, value, value, value, value, value,
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, send_data_cmd, sizeof(send_data_cmd), true);
    for (; n > sizeof(value_unroll); n -= sizeof(value_unroll)) {
        i2c_master_write(cmd, value_unroll, sizeof(value_unroll), true);
    }
    if (n > 0) {
        i2c_master_write(cmd, value_unroll, n, true);
    }
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t
ssd1306_set_range(i2c_port_t port, uint8_t col_min, uint8_t col_max, uint8_t page_min, uint8_t page_max) {
    uint8_t set_range_cmd[] = {
        0x78, 0x00,
        SSD1306_COLUMN_RANGE,
        col_min,
        col_max,
        SSD1306_PAGE_RANGE,
        page_min,
        page_max,
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, set_range_cmd, sizeof(set_range_cmd), true);
    i2c_master_stop(cmd);
    esp_err_t status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    return status;
}

esp_err_t
ssd1306_clear(i2c_port_t port) {
    esp_err_t status = ssd1306_set_range(port, 0, 127, 0, 3);
    if (status != ESP_OK) {
        return status;
    }
    return ssd1306_memset(port, 0, 128 * 4);
}

esp_err_t
ssd1306_init(i2c_port_t port, int sda_io, int scl_io) {
    static uint8_t init_cmd[] = {
        0x78, 0x00,
        SSD1306_DISPLAY_OFF,
        SSD1306_LAST_ROW, 0x3f,
        SSD1306_CHARGEPUMP, 0x14,
        SSD1306_ADDRESSING_MODE, 0,
        SSD1306_COM_PINS, 0x02,
        SSD1306_DISPLAY_RAM,
        SSD1306_DISPLAY_NORMAL,
        SSD1306_CONTRAST, 0x7f,
        SSD1306_NO_HORIZONTAL_FLIP,
        SSD1306_VSCAN_INC,
        SSD1306_DISPLAY_ON,
    };

    esp_err_t status;
    i2c_config_t conf;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_io;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl_io;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    //conf.master.clk_speed = 400000; // esp32
    conf.clk_stretch_tick = 300; // esp8266: 300 ticks, slave may hold down the clock this long

    //status = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0); // esp32
    status = i2c_driver_install(port, I2C_MODE_MASTER); // esp8266
    if (status != ESP_OK) {
        printf("i2c_driver_install failed; status=0x%02x\n", status);
        return status;
    }
    status = i2c_param_config(port, &conf);
    if (status != ESP_OK) {
        printf("i2c_param_config failed; status=0x%02x\n", status);
        return status;
    }

    i2c_cmd_handle_t cmd;

    // (Re)initialize the display
    // NOTE: Don't bother resetting values we never change. Noone else changes them either.
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, init_cmd, sizeof(init_cmd), true);
    i2c_master_stop(cmd);
    status = i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
    if (status != ESP_OK) {
        printf("ssd1306_init failed; status=0x%02x\n", status);
        return status;
    }
    ssd1306_set_range(port, 0x00, 0x7f, 0, 3);
    ssd1306_memset(port, 0, 128 * 64 / 8);

#ifdef TEST_PATTERNS
    // Binary pattern to page 0 (== rows 0..7)
    ssd1306_set_range(port, 0x00, 0x7f, 0, 0);
    for (uint16_t i = 0; i < 0x80; ++i) {
        ssd1306_send_data_byte(port, i);
    }

    // Sine pattern to page 1..7 (== rows 8..63)
    ssd1306_set_range(port, 0x00, 0x7f, 1, 7);
    for (uint16_t page = 0; page < 7; ++page) {
        for (uint16_t i = 0; i < 0x80; ++i) {
            int y = (int)(24 + (23 + sin(i * 3.1415926 / 64)));
            y -= (page << 3);
            if ((0 <= y) && (y < 8)) {
                ssd1306_send_data_byte(port, 1 << y);
            }
            else {
                ssd1306_send_data_byte(port, 0);
            }
        }
    }
#endif // TEST_PATTERNS

    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

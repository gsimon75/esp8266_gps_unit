#include "ssd1306.h"
#include <stdio.h>

// I2C header
// 0x00: slave address = 0,1,1,1, 1,0,<SA0>,<R/#W> = 0x78
// 0x01: control byte  = <Co>,<D/#C>,0,0, 0,0,0,0  = 0x00, 0x40, 0x80, 0xc0


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
    esp_err_t status = ssd1306_set_range(port, 0, 127, 0, 7);
    if (status != ESP_OK) {
        return status;
    }
    return ssd1306_memset(port, 0, 128 * 8);
}

// send a dummy packet, don't even check the results
static void
send_dummy(i2c_port_t port) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write(cmd, "\x00", 1, false);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(port, cmd, 1000);
    i2c_cmd_link_delete(cmd);
}


esp_err_t
ssd1306_init(i2c_port_t port, int sda_io, int scl_io) {
    static uint8_t init_cmd[] = {
        0x78, 0x00,
        SSD1306_DISPLAY_OFF,
        SSD1306_FREQ_DIV, 0x80,
        SSD1306_MULTIPLEX_RATIO, 0x3f,
        SSD1306_OFFSET_ROWS, 0x00,
        SSD1306_TOP_LINE + 0x00,
        SSD1306_CHARGEPUMP, 0x14,
        SSD1306_HORIZONTAL_FLIP,
        SSD1306_VERTICAL_FLIP,
        SSD1306_COM_PINS, 0x12,
        SSD1306_CONTRAST, 0x7f,
        SSD1306_PRECHARGE, 0xf1,
        SSD1306_VCOM_DESELECT, 0x40,
        SSD1306_DISPLAY_RAM,
        SSD1306_DISPLAY_NORMAL,
        SSD1306_COLUMN_START_LOW + 0x0,
        SSD1306_COLUMN_START_HIGH + 0x0,
        SSD1306_ADDRESSING_MODE, 0,
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

#if defined(CONFIG_IDF_TARGET_ESP32)
    status = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
#elif defined(CONFIG_IDF_TARGET_ESP8266)
    status = i2c_driver_install(port, I2C_MODE_MASTER);
#else
#   error "Unknown platform!"
#endif 

    if (status != ESP_OK) {
        printf("i2c_driver_install failed; status=0x%02x\n", status);
        return status;
    }
    status = i2c_param_config(port, &conf);
    if (status != ESP_OK) {
        printf("i2c_param_config failed; status=0x%02x\n", status);
        return status;
    }
    send_dummy(port);

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
    ssd1306_set_range(port, 0x00, 0x7f, 0, 7);
    ssd1306_memset(port, 0, 128 * 8);

    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

#include "gps.h"
#include "line_reader.h"
#include "oled_stdout.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <esp_log.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "gps";

#define BUF_SIZE 256
static QueueHandle_t uart0_queue;

static size_t available;

static ssize_t
gps_source(void *arg __attribute__((unused)), unsigned char *buf, size_t len) {
    if (len > available) {
        len = available;
    }
    if (len > 0) {
        uart_read_bytes(UART_NUM_0, buf, len, portMAX_DELAY);
        available -= len;

        buf[len] = '\0';
    }
    return len;
}

static void
uart_event_task(void *pvParameters) {

    line_reader_t *lr = lr_new(BUF_SIZE, gps_source, NULL);

    while (1) {
        uart_event_t event;
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA: {
                    ESP_LOGD(TAG, "UART_DATA %d bytes", event.size);
                    available = event.size;
                    unsigned char *line;
                    while ((line = lr_read_line(lr))) { 
                        ESP_LOGI(TAG, "%s", line);
                        if (!strncmp(line + 3, "RMC", 3)) {
                            lcd_gotoxy(0, 2);
                            printf("%s", line);
                            fflush(stdout);
                        }
                    }
                    break;
                }

                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "hw fifo overflow");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "ring buffer full");
                    uart_flush_input(UART_NUM_0);
                    xQueueReset(uart0_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "uart parity error");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "uart frame error");
                    break;

                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    lr_free(lr);
    lr = NULL;
    vTaskDelete(NULL);
}

esp_err_t
gps_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, UART_FIFO_LEN + 1, 0, 100, &uart0_queue, 0);

    xTaskCreate(uart_event_task, "gps", 2048, NULL, 12, NULL);
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

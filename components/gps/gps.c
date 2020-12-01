#include "gps.h"

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

static void
uart_event_task(void *pvParameters) {
    char *dtmp = (char *) malloc(BUF_SIZE);

    while (1) {
        uart_event_t event;
        if (xQueueReceive(uart0_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(UART_NUM_0, dtmp, event.size, portMAX_DELAY);
                    dtmp[event.size] = '\0';

                    char *msg = dtmp;
                    char *eol;
                    do {
                        eol = strchr(msg, '\n');
                        if (eol) {
                            eol[((dtmp < eol) && (eol[-1] == '\r')) ? -1 : 0] = '\0'; // replace (cr)lf with nul
                        }

                        if (*msg) {
                            ESP_LOGI(TAG, "%s", msg);
                        }

                        if (eol) {
                            msg = eol + 1;
                        }
                    } while (eol);
                    break;

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
    free(dtmp);
    dtmp = NULL;
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
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, BUF_SIZE * 2, 100, &uart0_queue, 0);

    xTaskCreate(uart_event_task, "gps", 2048, NULL, 12, NULL);
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

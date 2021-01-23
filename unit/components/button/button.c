#include "button.h"
#include "gps.h"
#include "admin_mode.h"
#include "location_reporter.h"
#include "misc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <driver/gpio.h>

#include <esp_system.h>
#include <esp_event_loop.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include <esp_log.h>

#define GPIO_BUTTON     0

static const char *TAG = "button";

static xQueueHandle gpio_evt_queue = NULL;
static TimerHandle_t gpio_debounce_timer[GPIO_NUM_MAX] = { NULL };

static void
wifi_stop(void) {
    esp_event_loop_set_cb(NULL, NULL);
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
}

static void
gpio_isr_handler(void *arg) {
    if (arg) {
        xTimerResetFromISR((TimerHandle_t)arg, 0);
    }
}


static void
gpio_debounce_expired(TimerHandle_t xTimer) {
    uint8_t gpio_num = (uintptr_t)pvTimerGetTimerID(xTimer);
    if (gpio_num < GPIO_NUM_MAX) {
        if (gpio_get_level(gpio_num)) {
            gpio_num |= 0x80;
        }
        xQueueSend(gpio_evt_queue, &gpio_num, 0);
    }
}

static bool in_admin_mode = false;
static bool keep_running = false;

static void
gpio_process_task(void *arg)
{
    for (keep_running = true; keep_running; ) {
        uint8_t io_num;
        if (!xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGE(TAG, "GPIO queue error");
            break;
        }
        ESP_LOGI(TAG, "GPIO[%d]=%d", io_num & 0x7f, io_num & 0x80);
        switch (io_num) {
            case GPIO_BUTTON: {
                if (in_admin_mode) {
                    break;
                }
                ESP_LOGI(TAG, "Entering admin mode");
                in_admin_mode = true;
                location_reporter_stop();
                gps_stop();
                wifi_stop();
                button_stop();
                wait_idle();
                admin_mode_start();
                break;
            }
        }
    }
    ESP_LOGI(TAG, "Finished");
    vTaskDelete(NULL);
}


static void
gpio_debouncer_add(int gpio_num, int debounce_ms) {
    if (gpio_debounce_timer[gpio_num]) {
        xTimerChangePeriod(gpio_debounce_timer[gpio_num], pdMS_TO_TICKS(debounce_ms), 0);
    }
    else {
        gpio_debounce_timer[gpio_num] = xTimerCreate("gpio", pdMS_TO_TICKS(debounce_ms), pdFALSE, (void*)gpio_num, gpio_debounce_expired);
        gpio_isr_handler_add(gpio_num, gpio_isr_handler, gpio_debounce_timer[gpio_num]);
    }
}


static void
gpio_debouncer_remove(int gpio_num) {
     gpio_isr_handler_remove(gpio_num);
     xTimerDelete(gpio_debounce_timer[gpio_num], 0);
     gpio_debounce_timer[gpio_num] = NULL;
}


esp_err_t
button_start(void) {
    gpio_evt_queue = xQueueCreate(GPIO_NUM_MAX, sizeof(uint8_t));
    BaseType_t res = xTaskCreate(gpio_process_task, "gpio", 3072, NULL, 10, NULL);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task; res=%d", res);
        return ESP_FAIL;
    }
    gpio_install_isr_service(0);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1 << GPIO_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);
    gpio_debouncer_add(GPIO_BUTTON, 300);
    ESP_LOGI(TAG, "Started");
    return ESP_OK;
}

esp_err_t
button_stop(void) {
    if (keep_running) {
        keep_running = false;
        // NOTE: don't wait for the task to finish, because it will call this, so it would cause a deadlock
        gpio_debouncer_remove(GPIO_BUTTON);
        gpio_isr_handler_remove(0);
    }
    ESP_LOGI(TAG, "Stopped");
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:

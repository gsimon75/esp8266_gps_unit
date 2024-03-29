#ifndef MAIN_H
#define MAIN_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

extern EventGroupHandle_t main_event_group;

#define IDLE_TASK_ACTIVE    BIT0
#define WIFI_CONNECTED_BIT  BIT1
#define OTA_CHECK_DONE_BIT  BIT2
#define GOT_GPS_FIX_BIT     BIT3
#define GOT_GPS_TIME_BIT    BIT4
#define LREP_RUNNING_BIT    BIT5

#define SSD1306_I2C I2C_NUM_0

#endif // MAIN_H
// vim: set sw=4 ts=4 indk= et si:

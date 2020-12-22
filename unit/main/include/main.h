#ifndef MAIN_H
#define MAIN_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t main_event_group;

/* The event group allows multiple bits for each event, but we only care about one event - are we connected to the AP with an IP? */
#define WIFI_CONNECTED_BIT  BIT0
#define OTA_CHECK_DONE_BIT  BIT1
#define LREP_RUNNING_BIT    BIT2
#define GOT_GPS_FIX_BIT     BIT3

#define GPIO_BUTTON     0
#define SSD1306_I2C I2C_NUM_0

#endif // MAIN_H
// vim: set sw=4 ts=4 indk= et si:

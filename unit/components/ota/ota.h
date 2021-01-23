#ifndef OTA_H
#define OTA_H

#include <esp_system.h>

esp_err_t ota_check_start(void);
void ota_check_task(void*);

#endif // OTA_H
// vim: set sw=4 ts=4 indk= et si:

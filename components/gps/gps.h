#ifndef GPS_H
#define GPS_H

#include <esp_system.h>
#include <esp_log.h>

typedef struct {
    bool is_valid;
    double datetime;
    float latitude, longitude, speed_kph, azimuth;
} gps_fix_t;

extern gps_fix_t gps_fix;

esp_err_t gps_init(void);

#endif // GPS_H
// vim: set sw=4 ts=4 indk= et si:

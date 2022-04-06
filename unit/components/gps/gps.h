#ifndef GPS_H
#define GPS_H

#include <esp_system.h>
#include <esp_log.h>

#define USE_AGPS

typedef enum {
    GPS_INIT,   // no answer yet, (re)init in progress
    GPS_NOFIX,  // comm ok, but no data whatsoever
    GPS_TIME,   // only gps time
    GPS_COARSE, // got position, but too diluted
    GPS_OK,     // all fine
    GPS_MAX
} gps_status_t;

typedef struct {
    uint64_t time_usec;
    float latitude, longitude, speed_kph, azimuth;
} gps_fix_t;

extern gps_fix_t gps_fix;
extern gps_status_t gps_status;
extern const char* gps_status_names[];

esp_err_t gps_start(void);
esp_err_t gps_add_agps(const uint8_t *data, size_t datalen);
esp_err_t gps_stop(void);


#endif // GPS_H
// vim: set sw=4 ts=4 indk= et si:

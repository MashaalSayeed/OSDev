#pragma once

#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;


rtc_time_t rtc_read_time();
void rtc_install();

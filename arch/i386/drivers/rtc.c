#include "drivers/rtc.h"
#include "io.h"

uint8_t rtc_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void rtc_write(uint8_t reg, uint8_t value) {
    outb(CMOS_ADDRESS, reg);
    outb(CMOS_DATA, value);
}

uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

rtc_time_t rtc_read_time() {
    rtc_time_t time;
    time.second = bcd_to_binary(rtc_read(0x00));
    time.minute = bcd_to_binary(rtc_read(0x02));
    time.hour = bcd_to_binary(rtc_read(0x04));
    time.day = bcd_to_binary(rtc_read(0x07));
    time.month = bcd_to_binary(rtc_read(0x08));
    time.year = bcd_to_binary(rtc_read(0x09)) + 2000;
    return time;
}

void rtc_install() {
    // Enable the RTC interrupt
    uint8_t prev = rtc_read(0x8B);
    rtc_write(0x8B, prev | 0x40);
}
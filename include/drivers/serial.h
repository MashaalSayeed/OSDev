#pragma once

void init_serial();
void serial_write(char c);
char serial_read();
void log_to_serial(const char* data);
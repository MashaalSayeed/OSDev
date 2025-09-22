#pragma once

void init_serial();
void serial_write_char(char c);
void serial_write(const char* data);
char serial_read();
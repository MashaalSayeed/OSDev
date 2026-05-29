#pragma once

int file_exists(const char *path);
int read_file_contents(int fd, char *buf, int len);
void setup_test_files(void);

#pragma once

#include <stddef.h>
#include <stdint.h>

void memset(void *dest, char val, uint32_t count);
void memcpy(void *dest, const void *src, uint32_t count);
uint8_t memcmp(void *dest, const void *src, uint32_t count);

size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
void strncpy(char *dest, const char *src, uint32_t count);
int strcmp(const char *s1, const char *s2);

void int_to_ascii(uint32_t num, char *buffer);
void hex_to_ascii(uint32_t num, char *buffer);
void bin_to_ascii(uint32_t num, char *buffer);
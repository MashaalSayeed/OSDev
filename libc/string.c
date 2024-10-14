#include "libc/string.h"

size_t strlen(const char *s) {
    size_t i = 0;
    for(; *s; i++) {
        s++;
    }
    return i;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while(*src) {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return ret;
}

int strcmp(const char *s1, const char *s2) {
    while(*s1 && *s2) {
        if(*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

void int_to_ascii(uint32_t num, char *buffer) {
    // Handle zero case
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char *ptr = buffer;
    char *ptr1 = buffer; // Pointer to the start of the string
    int temp;

    while (num != 0) {
        temp = num % 10;
        num /= 10;
        *ptr++ = temp + '0';
    }
    *ptr = '\0';                    // Null-terminate the string

    reverse(ptr1);
}

void hex_to_ascii(uint32_t num, char* buffer) {
    char *ptr = buffer;
    char *ptr1 = buffer; // Pointer to the start of the string

    *ptr++ = '0';
    *ptr++ = 'x';

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    int temp;
    while (num != 0) {
        temp = num % 16;
        num /= 16;
        if (temp < 10) {
            *ptr++ = temp + '0';
        } else {
            *ptr++ = temp - 10 + 'A';
        }
    }
    *ptr = '\0';

    reverse(ptr1 + 2);
}
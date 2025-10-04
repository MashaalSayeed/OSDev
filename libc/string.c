#include "libc/string.h"

void memset(void *dest, char val, uint32_t count){
    char *temp = (char*) dest;
    for (; count != 0; count --){
        *temp++ = val;
    }
}

void memcpy(void *dest, const void *src, uint32_t count){
    char *dest8 = (char*) dest;
    const char *src8 = (const char*) src;
    for (; count != 0; count --){
        *dest8++ = *src8++;
    }
}

uint8_t memcmp(void *dest, const void *src, uint32_t count){
    char *dest8 = (char*) dest;
    const char *src8 = (const char*) src;
    for (; count != 0; count --){
        if (*dest8++ != *src8++){
            return 1;
        }
    }
    return 0;
}

// memmove is used to copy memory from one location to another, handling overlapping regions correctly.
// If the source and destination regions overlap, it copies backwards to avoid overwriting data before it
// is read. If they do not overlap, it copies forwards as usual.
void *memmove(void *dest, const void *src, uint32_t count) {
    if (!dest || !src) return dest;  // Optional safety check

    char *d = (char *)dest;
    const char *s = (const char *)src;

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        for (uint32_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (int i = count - 1; i >= 0; i--) {
            d[i] = s[i];
        }
    }

    return dest;
}

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

void strncpy(char *dest, const char *src, uint32_t count) {
    uint32_t i = 0;
    for (; i < count && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    for (; i < count; i++) dest[i] = '\0';
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

int strncmp(const char *s1, const char *s2, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
    }
    return 0;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) {
        dest++;
    }
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return ret;
}

char *strncat(char *dest, const char *src, uint32_t count) {
    char *ret = dest;
    while (*dest) {
        dest++;
    }
    for (uint32_t i = 0; i < count; i++) {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return ret;
}

void reverse(char s[]) {
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

char *strchr(const char *s, char c) {
    while (*s) {
        if (*s == c) {
            return (char *)s; // Return pointer to the first occurrence
        }
        s++;
    }
    // Check if the null terminator is the target character
    if (c == '\0') {
        return (char *)s;
    }
    return NULL; // Return NULL if character not found
}

char *strrchr(const char *s, char c) {
    char *last = NULL;
    while (*s) {
        if (*s == c) {
            last = (char *)s;
        }
        s++;
    }
    return last;
}

char *strtok(char *str, const char *delim) {
    static char *last = NULL;
    if (delim == NULL) return NULL;

    if (str) {
        last = str;
    } else if (!last) {
        return NULL; // If both str and last are NULL, return NULL
    }

    // Skip leading delimiters
    str = last;
    while (*str && strchr(delim, *str)) {
        str++;
    }

    if (*str == '\0') {
        last = NULL; // No more tokens
        return NULL;
    }

    // Find the end of the current token
    char *token_start = str;
    while (*str && !strchr(delim, *str)) {
        str++;
    }

    if (*str) {
        *str = '\0';   // Null-terminate the token
        last = str + 1; // Update last to point to the next character
    } else {
        last = NULL; // No more tokens
    }

    return token_start;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (str) *saveptr = str;
    if (!*saveptr) return NULL;

    // Skip leading delimiters
    char *token_start = *saveptr;
    while (*token_start && strchr(delim, *token_start)) token_start++;

    if (!*token_start) {
        *saveptr = NULL;
        return NULL;
    }

    // Find the end of the token
    char *token_end = token_start;
    while (*token_end && !strchr(delim, *token_end)) token_end++;

    if (*token_end) {
        *token_end = '\0';
        *saveptr = token_end + 1;
    } else {
        *saveptr = NULL;
    }

    return token_start;
}

char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

void int_to_ascii(int num, char *buffer) {
    // Handle zero case
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char *ptr = buffer;
    char *ptr1 = buffer; // Pointer to the start of the string
    int is_negative = 0;

    // Handle negative numbers
    if (num < 0) {
        is_negative = 1;
        num = -num; // Convert to positive for processing
    }

    while (num != 0) {
        *ptr++ = (num % 10) + '0';
        num /= 10;
    }

    if (is_negative) *ptr++ = '-';

    *ptr = '\0';
    reverse(ptr1);
}

void hex_to_ascii(unsigned int num, char* buffer) {
    char *ptr = buffer;
    char *ptr1 = buffer; // Pointer to the start of the string

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

    reverse(ptr1);
}

void bin_to_ascii(unsigned int num, char* buffer) {
    char *ptr = buffer;
    char *ptr1 = buffer; // Pointer to the start of the string

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    int temp;
    while (num != 0) {
        temp = num % 2;
        num /= 2;
        *ptr++ = temp + '0';
    }
    *ptr = '\0';

    reverse(ptr1);
}
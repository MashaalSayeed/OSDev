#include <stddef.h>
#include "kernel/string.h"
#include "kernel/kheap.h"
#include "libc/string.h"

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *ret = (char *)kmalloc(len + 1);
    strcpy(ret, s);
    return ret;
}

char *strndup(const char *s, size_t n) {
    char *ret = (char *)kmalloc(n + 1);
    strncpy(ret, s, n);
    ret[n] = '\0';
    return ret;
}
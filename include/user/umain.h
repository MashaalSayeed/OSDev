#pragma once

#include <stdint.h>

typedef struct {
    const char *name;
    void (*func)(char **args);
} command_t;

void user_program();
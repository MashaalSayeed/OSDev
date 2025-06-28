#ifndef PROC_INFO_H
#define PROC_INFO_H

#include <stdint.h>

typedef enum {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
} proc_state_t;

typedef struct {
    int pid;
    int ppid;
    char name[32];
    proc_state_t state;
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t brk;
    uint32_t stack;
} proc_info_t;

#endif
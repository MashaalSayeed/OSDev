#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel/isr.h"

#define PROCESS_NAME_MAX_LEN 32

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} process_status_t;

// PCB (Process Control Block)
typedef struct process {
    size_t pid;
    process_status_t status;
    char process_name[PROCESS_NAME_MAX_LEN];

    registers_t* context;
    void *root_page_table;
    struct process* next;
} process_t;

void scheduler_init();
registers_t* schedule(registers_t* context);
process_t* create_process(char *process_name, void (*entry_point)());
void kill_process(process_t *process);

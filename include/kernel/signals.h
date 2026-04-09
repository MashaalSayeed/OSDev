#pragma once

#include <stdint.h>
#include "kernel/isr.h"
#include "common/signals.h"

// Forward declaration to avoid circular dependency
typedef struct process process_t;

// Signal frame saved on user stack during handler dispatch
typedef struct {
    uint32_t    retaddr;    // points to sigreturn trampoline
    uint32_t    sig;        // signal number — argument to handler

    // ONLY user-mode fields:
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t eip;
    uint32_t eflags, useresp;
    uint32_t cs, ss;
} signal_frame_t;

void do_signal(registers_t *regs);
void sys_sigreturn(registers_t *regs);
void map_signal_trampoline(process_t *proc);
#include "kernel/signals.h"
#include "kernel/paging.h"
#include "kernel/process.h"
#include "kernel/pmm.h"
#include "kernel/syscall.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/system.h"

extern page_directory_t* kpage_dir;
void map_signal_trampoline(process_t *proc) {
    uint32_t frame = pmm_alloc_block();
    if (!frame) {
        kprintf(ERROR, "map_signal_trampoline: failed to allocate frame\n");
        return;
    }

    uint32_t phys = frame * PAGE_SIZE;
    uint8_t trampoline[] = {
        0xB8,
        (SYS_SIGRETURN & 0xFF),
        ((SYS_SIGRETURN >> 8)  & 0xFF),
        ((SYS_SIGRETURN >> 16) & 0xFF),
        ((SYS_SIGRETURN >> 24) & 0xFF),
        0xCD, 0x80,
        0xF4
    };

    page_table_entry_t *existing = get_page(SIGRETURN_TRAMPOLINE_ADDR, 0, proc->root_page_table);
    if (existing && existing->present) {
        existing->present = 0;
        existing->frame = 0;
        invalidate_page(SIGRETURN_TRAMPOLINE_ADDR);
    }

    map_memory(proc->root_page_table, SIGRETURN_TRAMPOLINE_ADDR, phys, PAGE_SIZE, PAGE_USER | PAGE_RW);
    copy_to_user(proc->root_page_table, SIGRETURN_TRAMPOLINE_ADDR, trampoline, sizeof(trampoline));
}

void sys_sigreturn(registers_t *regs) {
    process_t *proc = get_current_process();

    // The signal_frame_t is at the current user esp
    uint32_t frame_useresp = regs->useresp + 4;
    signal_frame_t frame;
    copy_from_user(proc->root_page_table, &frame, frame_useresp, sizeof(signal_frame_t));
    
    // Restore the original context directly into regs
    // (regs is the actual kernel stack frame that iret will use)
    regs->eax     = frame.eax;
    regs->ebx     = frame.ebx;
    regs->ecx     = frame.ecx;
    regs->edx     = frame.edx;
    regs->esi     = frame.esi;
    regs->edi     = frame.edi;
    regs->ebp     = frame.ebp;
    regs->eip     = frame.eip;
    regs->eflags  = frame.eflags;
    regs->useresp = frame.useresp;
    regs->cs      = frame.cs;
    regs->ss      = frame.ss;
}

void setup_signal_frame(registers_t *regs, int sig, uint32_t handler) {
    // Save the current userspace registers_t onto user stack
    // so sigreturn can restore them
    process_t *proc = get_current_process();
    uint32_t useresp = regs->useresp;          // original user esp
    useresp -= sizeof(signal_frame_t); // make room for signal frame

    uint32_t frame_addr    = useresp;
    
    // Push a signal_frame_t onto the user stack
    signal_frame_t frame;
    frame.retaddr = SIGRETURN_TRAMPOLINE_ADDR;  // where handler returns to
    frame.sig     = sig;
    frame.eax      = regs->eax;
    frame.ebx      = regs->ebx;
    frame.ecx      = regs->ecx;
    frame.edx      = regs->edx;
    frame.esi      = regs->esi;
    frame.edi      = regs->edi;
    frame.ebp      = regs->ebp;
    frame.eip      = regs->eip;
    frame.eflags   = regs->eflags;
    frame.useresp  = regs->useresp; // So handler can know where original stack was
    frame.cs       = regs->cs;
    frame.ss       = regs->ss;
    
    useresp -= 4; // Push handler ABI frame
    copy_to_user(proc->root_page_table, useresp, &sig, 4);

    useresp -= 4;
    uint32_t tramp = SIGRETURN_TRAMPOLINE_ADDR;
    copy_to_user(proc->root_page_table, useresp, &tramp, 4);

    // Copy frame to userspace
    copy_to_user(proc->root_page_table, frame_addr, &frame, sizeof(frame));

    // Now redirect iret to the signal handler
    regs->useresp = useresp;
    regs->eip = handler;
}

static void kernel_halt() {
    for (;;) asm volatile("cli; hlt");
}

// Called from irq_common_stub with the registers_t* just before iret
void do_signal(registers_t *regs) {
    if (regs->cs == 0x08) return;  // kernel thread, skip
    process_t *proc = get_current_process();
    if (!proc) return;
    
    uint32_t deliverable = proc->pending_signals & ~proc->signal_mask;
    if (!deliverable) return;
    
    // Pick lowest pending signal
    int sig = __builtin_ctz(deliverable);
    proc->pending_signals &= ~(1 << sig);
    
    sighandler_t handler = proc->signal_handlers[sig].handler;
    
    kprintf(DEBUG, "Delivering signal %d to process %d (handler=%x)\n", sig, proc->pid, handler);
    if (handler == SIG_DFL) {
        // Default action — most signals kill the process
        switch (sig_default_action(sig))
        {
        case SIGACT_IGN:
        case SIGACT_CONT:
            return;
        case SIGACT_STOP:
        case SIGACT_CORE:
        case SIGACT_TERM:
            kprintf(DEBUG, "Default action for signal %d is to kill process %d\n", sig, proc->pid);
            kill_process(proc, sig);
            regs->eip = (uint32_t)kernel_halt; // Just in case kill_process returns, ensure we don't return to user code
            // schedule(regs);
        default:
            break;
        }

        return;
    }
    if (handler == SIG_IGN) return;
    
    setup_signal_frame(regs, sig, (uint32_t)handler);
}
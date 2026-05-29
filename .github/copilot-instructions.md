# ZineOS – Copilot Instructions

## Project Overview
ZineOS is a hobby x86 (i386/i686) OS kernel written in C and NASM assembly, booted via GRUB Multiboot2. It runs in QEMU. The codebase targets a **cross-compiler** (`i686-elf-gcc`/`i686-elf-ld`) — never use the host system's gcc.

## Directory Layout
| Path | Purpose |
|------|---------|
| `arch/i386/` | Architecture-specific: boot entry (`boot.s`), GDT/IDT asm, hardware drivers |
| `kernel/` | Kernel subsystems: fs, gui, memory, scheduler, cpu (syscalls/exceptions) |
| `kernel/gui/` | Framebuffer + window manager device driver (`/DEV/WM`) |
| `include/` | Shared headers — `kernel/`, `drivers/`, `user/`, `libc/`, `common/` |
| `user/` | Ring-3 programs (`core/`), user-space libs (`lib/`), `crt0.s`, linker script |
| `user/core/` | User apps; each app has its own Makefile (auto-discovered) |
| `libc/` | Kernel-side minimal libc built as `libc.a` |
| `scripts/` | Setup/build/install helpers |
| `iso/` | Output ISO + GRUB config; `zdisk.img` is the FAT32 user-binary disk |
| `build/` | All compiled objects and binaries (generated, do not edit) |

## Key Build Commands
```bash
make                  # Full build: libc → userland → kernel ISO → FAT32 disk image
make run              # Boot in QEMU (text mode)
make run GUI=1        # Boot with VBE framebuffer (1024×768×32bpp)
make debug            # QEMU + GDB server on :1234 (uses i386-elf-gdb)

make userland         # Rebuild only user apps (binaries in build/i386/user/bin)
./scripts/install.sh  # Copy built user binaries into zdisk.img (macOS: uses hdiutil)

./scripts/setup.sh    # First-time cross-toolchain + GRUB install (run once)
./scripts/build.sh    # Build GRUB from source (run after setup.sh)
```
Serial output is written to `serial_output.log` (QEMU `-serial file:...`). Check it for kernel boot diagnostics.

## Userland Build + Musl
- Default toolchain for user apps is the cross-compiler (`i686-elf-gcc`).
- Musl is available at `/usr/local/i386-zineos-musl` (see `user/Makefile.conf`).
- To build a user app with musl, create an empty `USE_MUSL` file in its app dir:
	- `user/core/<app>/USE_MUSL`
	- The top-level `user/Makefile` auto-detects these and builds them with musl.
- Musl apps still link `libuser.a` to use ZineOS wrappers (WM, syscalls).

## Userland Install Flow
1. `make userland`
2. `./scripts/install.sh` to copy `/BIN/*` into `zdisk.img`
3. Boot with `make run` or `make run GUI=1`
Serial output is written to `serial_output.log` (QEMU `-serial file:...`). Check it for kernel boot diagnostics.

## Higher-Half Kernel
The kernel is **linked at `0xC0000000`** but the Multiboot2 header loads at `0x10000`. The linker script (`arch/i386/boot/linker.ld`) uses `AT()` to place physical addresses while keeping virtual addresses above `0xC0000000`. When computing physical addresses at early boot, use `virtual - 0xC0000000`. The global `kpage_dir` holds the kernel page directory.

## GUI vs. Text Mode
Pass `GUI=1` to make — this injects `-D ENABLE_GUI` into `ASM_FLAGS`, which compiles the Multiboot2 framebuffer tag into `boot.s`. At runtime `is_gui_enabled` gates GUI setup (framebuffer, SHM, `/DEV/WM`) vs. the plain TTY path.

## GUI/WM Architecture (User Space)
- `/DEV/WM` is the compositor mailbox device. The first client that sends `WM_REQ_CONNECT` becomes the compositor.
- Clients create windows via `wm_create_window()` which maps an SHM pixel buffer.
- Use `wm_flush()` to mark regions dirty.
- Keyboard/mouse events are delivered as `WM_EVENT_KEY` / `WM_EVENT_MOUSE` events.
- The framebuffer mapping syscall is `syscall_fb_map()` (mapped at `0x90000000`).

## VFS Design Pattern
Two distinct operation tables per file:
- **`vfs_inode_operations`** — filesystem-specific (lookup, create, mkdir, readdir…) attached to `vfs_inode_t.inode_ops`
- **`vfs_file_operations`** — per-open-file (read, write, seek, close) attached to `vfs_file_t.file_ops`

Console (FDs 0–2) uses a global `console_fds[3]` with `vfs_console_ops`; per-process FDs start at 3.  
To add a new filesystem: implement both op tables, register a `vfs_mount_t`, and mount via `vfs_mount_list`.

## Syscall Convention
- **User → kernel**: `int $0x80`; syscall number in `EAX`, args in `EBX`, `ECX`, `EDX`.
- Numbers are in `include/common/syscall.h` (Linux-compatible where possible).
- User-space wrappers live in `user/lib/syscall.c`; kernel handlers in `kernel/cpu/syscall.c`.

### Common Syscalls in Use (Non-Exhaustive)
- Process: `fork`, `exec`, `waitpid`, `exit`, `getpid`, `getppid`
- File I/O: `open`, `read`, `write`, `close`, `getdents`, `stat`, `fstat`, `lseek`
- Memory: `brk`, `sbrk`, `mmap2`, `munmap`, `mprotect`
- Timing: `get_ticks`, `nanosleep`, `yield`
- GUI: `shm_create`, `shm_map`, `shm_unmap`, `shm_destroy`, `fb_map`

## Process & Scheduler
- `process_t` owns a linked list of `thread_t`; each thread has its own kernel stack (kmalloc'd) and user stack (mapped at `0xB0000000 - n*PROCESS_STACK_SIZE`).
- `PROCESS_FLAG_KERNEL` = ring 0, `PROCESS_FLAG_USER` = ring 3.
- Round-robin scheduler fires at 100 Hz (PIT). `schedule(registers_t*)` in `kernel/scheduler/scheduler.c` does the context switch via `switch_task()` (asm).
- `exec("/BIN/SHELL", NULL)` is the canonical way to launch a user ELF from kernel code.
- User heap starts at `USER_HEAP_START` (`0x400000`) and is capped by `PROCESS_MAX_HEAP_SIZE`.

## Include Path Conventions
All `#include` paths are relative to `include/`. Use angle brackets for all kernel headers:
```c
#include <kernel/vfs.h>       // kernel subsystem
#include <drivers/keyboard.h> // driver headers
#include <common/syscall.h>   // shared kernel/user constants
#include "user/dirent.h"      // user-space ABI types used in kernel
#include "libc/string.h"      // kernel-side libc
```

## Logging
- `kprintf(DEBUG|INFO|WARNING|ERROR, fmt, ...)` — levelled kernel logging (serial + TTY).
- `printf(fmt, ...)` — plain TTY output, also available in kernel.
- Prefer `kprintf` with an appropriate level for any new diagnostic output.
- Avoid `printf` in kernel code except for very early boot messages before `kprintf` is safe to use.
- Logs are written to `serial_output.log` (QEMU `-serial file:...`) — check this for all kernel boot and runtime diagnostics.

## Adding a New User Program
1. Create `user/core/<name>/` with a `Makefile` and your sources.
2. Use headers under `include/user/` and `include/libc/` (and app-local includes).
3. `user/Makefile` auto-discovers apps in `user/core/*/Makefile`.
4. If the app should use musl, add `user/core/<name>/USE_MUSL`.
5. Run `make userland` then `./scripts/install.sh` to deploy to `zdisk.img`.
6. The binary will be accessible at `/BIN/<NAME>` inside the OS.

# TODO PLAN

## Phase 0 — Kernel Stability
- [x] Fix waitpid — scheduler.c: pick_next_thread skips WAITING threads forever but never wakes them up
- [x] Add blocking sleep/wake queues — wait_queue_t + wait_queue_sleep/wake to locks.c
- [x] Make pipe reads blocking — pipe.c: sleep on wait_queue_t, wake on pipe_write
- [x] PMM: register multiboot memory map regions — uncomment add_memory_region() in pmm_init
- [x] Add spinlocks to kmalloc and PMM bitmap — prevent IRQ-driven corruption
- [x] Add sys_stat / sys_fstat — minimal stat_t struct + handlers
- [x] Add sys_get_ticks — expose PIT tick counter (100 Hz) as syscall 26
- [x] FAT32 mkdir — implement fat32_mkdir in fat32.c
- [x] Add copy_to_user — validate user pointers in syscall.c
- [x] nanosleep / sleep syscall — using tick counter, SLEEPING thread state
- [x] mmap / munmap — anonymous mmap via map_memory()

## Phase 1 — Core POSIX Syscall Layer (blocks most userspace programs)
- [x] Signals — pending_signals + signal_handlers[], sys_signal, sys_kill, trampoline
- [x] dup2 / fcntl(F_DUPFD) — shell redirection (>, >>, 2>&1), required by musl stdio
- [x] ioctl + TTY as VFS node — /DEV/TTY0, TIOCGWINSZ, TCGETS/TCSETS (blocks readline, editors)
- [x] rename syscall — needed for editors (save-to-temp pattern)
- [x] getdents64 — musl uses this, not getdents; directory listing breaks without it
- [x] sys_getcwd — musl calls this on startup, currently probably stubbed or broken
- [x] sys_readlink on /proc/self/exe — musl and many programs call this at startup

## Phase 2 — Shell & Process Model
- [x] Signals delivered on schedule() — check pending_signals before returning to userspace
- [x] SIGCHLD + waitpid(-1) — shell needs to reap any child, not just a specific pid
- [x] pipe + dup2 plumbing — shell pipelines (cmd1 | cmd2)
- [x] /DEV/NULL and /DEV/ZERO — many programs open these unconditionally
- [x] environ / execve envp — pass environment to exec'd processes
- [x] Session/pgrp stubs — sys_setsid, sys_getpgrp (musl calls these, can be no-ops for now)

## Phase 3 — Filesystem Hardening
- [ ] FAT32 rename — fat32_rename, required by editors and installers
- [ ] FAT32 truncate — ftruncate syscall + fat32_truncate
- [ ] /PROC virtual filesystem — /PROC/<pid>/status, /PROC/self/exe (unblocks readlink)
- [ ] /TMP backed by ramfs — mount at /TMP in vfs_init
- [ ] DEV device naming cleanup — /DEV/TTY0, /DEV/FB0, /DEV/KBD, /DEV/NULL, /DEV/ZERO
- [ ] ETC on FAT32 disk — hostname, version, APPS list

## Phase 4 — Desktop / WM
- [ ] Forward keyboard events to WM — wm_dev_push_input_event for keystrokes
- [ ] /DEV/TTY pseudo-terminal — pty_t master/slave pair (blocks terminal emulator)
- [ ] Window resizing — handle WM_REQ_RESIZE, destroy/recreate SHM, notify client
- [ ] Z-order / window stacking — ordered list instead of flat windows[] array
- [ ] PSF font ANSI color — ui_draw_string_colored
- [ ] Taskbar and app launcher — /ETC/APPS config file + launcher UI

## Phase 5 — Applications
- [ ] Terminal emulator — WM window + PTY + VT100/ANSI rendering
- [ ] File manager — getdents + icons/list + app launch
- [ ] Port doomgeneric — sys_fb_map, sys_get_ticks, sys_input_read, sys_audio_write
- [ ] Port Lua 5.4 — works with musl; needs /TMP, rename, working stdio

## Phase 6 — Networking
- [ ] virtio-net driver — PCI enumeration + virtqueue + DMA
- [ ] Minimal TCP/IP stack — lwIP port (needs malloc, select/poll stubs)
- [ ] telnetd or httpd — remote shell or file serving

## Phase 7 — Audio (optional)
- [ ] PC speaker (8254 channel 2) — pit_beep(freq, ms)
- [ ] AC97 / SB16 driver — DMA ring buffer + sys_audio_write

## Project Structure
- [ ] Separate kernel and user headers strictly — no kernel structs in userspace headers
- [ ] /PROC/<pid>/maps — needed for musl's dynamic linker and sanitizers
- [ ] Versioned disk image — bump zdisk.img to 256 MB
- [ ] sysinfo integration test — user/core/sysinfo as boot-time smoke test

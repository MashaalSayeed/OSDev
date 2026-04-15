#pragma once

#include <stdint.h>

/* Linux-compatible tty ioctl request numbers (i386) */
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414

/* Minimal Linux-compatible termios layout for TCGETS/TCSETS */
#define NCCS 19

typedef uint32_t tcflag_t;
typedef uint8_t cc_t;

typedef struct termios_linux {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
} termios_linux_t;

/* Minimal lflag bits used by tty line discipline */
#define TTY_LFLAG_ISIG    0x00000001u
#define TTY_LFLAG_ICANON  0x00000002u
#define TTY_LFLAG_ECHO    0x00000008u

/* termios c_cc[] indices (Linux-compatible ordering) */
#define VINTR   0
#define VQUIT   1
#define VERASE  2
#define VKILL   3
#define VEOF    4
#define VTIME   5
#define VMIN    6
#define VSWTC   7
#define VSTART  8
#define VSTOP   9
#define VSUSP   10
#define VEOL    11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT  15
#define VEOL2   16

typedef struct winsize_linux {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} winsize_linux_t;

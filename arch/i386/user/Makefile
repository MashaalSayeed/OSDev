ARCH=i386

CC=i686-elf-gcc
LD=i686-elf-ld
AS=nasm

CFLAGS=-ffreestanding -O2 -Wall -Wextra -O0 -I../../../include -std=gnu99 -g
LDFLAGS=-nostdlib -ffreestanding -O2 -lgcc -T linker.ld

BUILD_DIR=../../../build/$(ARCH)
USER_BUILD_DIR=$(BUILD_DIR)/user

USER_SRC=$(wildcard *.c) $(wildcard lib/*.c)
USER_OBJS=$(USER_SRC:%.c=$(USER_BUILD_DIR)/%.o)
LIBC_OBJS=$(BUILD_DIR)/libc/libc.a
OBJS=$(USER_OBJS) $(LIBC_OBJS)

USER_BIN=$(USER_BUILD_DIR)/user.bin

all: $(USER_BIN)

$(USER_BUILD_DIR):
	mkdir -p $(USER_BUILD_DIR) $(USER_BUILD_DIR)/lib

$(USER_BUILD_DIR)/%.o: %.c | $(USER_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ 

$(USER_BIN): $(OBJS) | $(USER_BUILD_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f $(USER_BUILD_DIR)
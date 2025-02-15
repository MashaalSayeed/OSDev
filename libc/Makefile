ARCH?=i386

CC=i686-elf-gcc
AR=i686-elf-ar
RANLIB=i686-elf-ranlib

CFLAGS?=-ffreestanding -O2 -Wall -Wextra -O0 -I../include -std=gnu99 -g

BUILD_DIR=../build/$(ARCH)
LIBC_BUILD_DIR=$(BUILD_DIR)/libc

LIBC_SRC = $(wildcard *.c)
LIBC_OBJS = $(LIBC_SRC:%.c=$(LIBC_BUILD_DIR)/%.o)
LIBC_A = $(LIBC_BUILD_DIR)/libc.a

all: $(LIBC_A)

$(LIBC_BUILD_DIR):
	mkdir -p $@

$(LIBC_BUILD_DIR)/%.o: %.c
	@echo "Compiling libc source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBC_A): $(LIBC_OBJS) | $(LIBC_BUILD_DIR)
	@echo "Building libc archive..."
	$(AR) rcs $@ $^
	$(RANLIB) $@

clean:
	rm -f $(LIBC_OBJS) $(LIBC_A)
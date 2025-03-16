ARCH=i386

CC=i686-elf-gcc
LD=i686-elf-ld
AS=nasm

CFLAGS=-ffreestanding -O2 -Wall -Wextra -O0 -I../../../include -std=gnu99 -g
LDFLAGS=-nostdlib -ffreestanding -O2 -lgcc -Tlinker.ld

BUILD_DIR=../../../build/$(ARCH)
USER_BUILD_DIR=$(BUILD_DIR)/user
CORE_BUILD_DIR=$(USER_BUILD_DIR)/core
LIB_BUILD_DIR=$(USER_BUILD_DIR)/lib
BIN_DIR=$(USER_BUILD_DIR)/bin

CORE_SRC=$(wildcard core/*.c)
LIB_SRC=$(wildcard lib/*.c)
LIB_OBJS=$(LIB_SRC:%.c=$(USER_BUILD_DIR)/%.o) $(BUILD_DIR)/libc/libc.a

PROGS = shell hello editor
PROG_OBJS = $(patsubst %, $(CORE_BUILD_DIR)/%.o, $(PROGS))
PROG_BINS = $(patsubst %, $(BIN_DIR)/%, $(PROGS))

.PHONY: all clean

all: $(PROG_BINS)

$(USER_BUILD_DIR):
	mkdir -p $(USER_BUILD_DIR) $(CORE_BUILD_DIR) $(LIB_BUILD_DIR) $(BIN_DIR)

$(CORE_BUILD_DIR)/%.o: core/%.c | $(USER_BUILD_DIR)
# @echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_BUILD_DIR)/%.o: lib/%.c | $(USER_BUILD_DIR)
# @echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(CORE_BUILD_DIR)/%.o $(LIB_OBJS) | $(USER_BUILD_DIR)
# @echo "Linking $< $(LIB_OBJS)"
	$(CC) $(LDFLAGS) $< $(LIB_OBJS) -o $@

clean:
	rm -rf $(USER_BUILD_DIR)
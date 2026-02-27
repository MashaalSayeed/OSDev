ARCH=i386

CC=i686-elf-gcc
LD=i686-elf-ld
AS=nasm

CFLAGS=-ffreestanding -O2 -Wall -Wextra -O0 -I../include -std=gnu99 -g
LDFLAGS=-nostdlib -nostartfiles -ffreestanding -O2 -lgcc -Tlinker.ld

BUILD_DIR=../build/$(ARCH)
USER_BUILD_DIR=$(BUILD_DIR)/user
CORE_BUILD_DIR=$(USER_BUILD_DIR)/core
LIB_BUILD_DIR=$(USER_BUILD_DIR)/lib
BIN_DIR=$(USER_BUILD_DIR)/bin

CORE_SRC=$(wildcard core/*.c)

# Recursively discover all C sources under user/lib/
# Map lib/*.c -> $(USER_BUILD_DIR)/lib/*.o
# Archive produced from library objects and linked into user programs
LIB_C_SRCS := $(shell find lib -name '*.c' 2>/dev/null)
LIB_OBJS := $(patsubst lib/%.c,$(LIB_BUILD_DIR)/%.o,$(LIB_C_SRCS))
LIB_ARCHIVE := $(USER_BUILD_DIR)/libuser.a

# System libc archive (prebuilt into build tree)
LIBSYS := $(BUILD_DIR)/libc/libc.a

PROGS = shell hello editor desktop notepad
PROG_OBJS = $(patsubst %, $(CORE_BUILD_DIR)/%.o, $(PROGS))
PROG_BINS = $(patsubst %, $(BIN_DIR)/%, $(PROGS))

.PHONY: all clean

all: $(PROG_BINS)

$(USER_BUILD_DIR):
	mkdir -p $(USER_BUILD_DIR) $(CORE_BUILD_DIR) $(LIB_BUILD_DIR) $(BIN_DIR)

$(CORE_BUILD_DIR)/%.o: core/%.c | $(USER_BUILD_DIR)
# @echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_BUILD_DIR)/crt0.o: ./crt0.s | $(USER_BUILD_DIR)
	$(AS) -f elf32 -o $@ $<

$(LIB_BUILD_DIR)/%.o: lib/%.c | $(USER_BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Produce an archive from all library objects (if any)
$(LIB_ARCHIVE): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	@if [ -n "$(LIB_OBJS)" ]; then \
		ar rcs $@ $^; \
	else \
		touch $@; \
	fi

$(BIN_DIR)/%: $(CORE_BUILD_DIR)/%.o $(USER_BUILD_DIR)/crt0.o $(LIB_ARCHIVE) $(LIBSYS) | $(USER_BUILD_DIR)
	$(CC) $(LDFLAGS) $< $(USER_BUILD_DIR)/crt0.o $(LIB_ARCHIVE) $(LIBSYS) -o $@

clean:
	rm -rf $(USER_BUILD_DIR)
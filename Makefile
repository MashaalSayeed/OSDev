ARCH ?= i386
SCAMARCH ?= i386
TARGET = i386-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld
GDB = $(TARGET)-gdb
AS = nasm

PWD := $(shell pwd | sed 's/ /\\ /g')

# Directories
ARCH_DIR = arch/$(ARCH)
BUILD_DIR = build/$(ARCH)
KERNEL_DIR = $(ARCH_DIR)/kernel
USER_DIR = $(ARCH_DIR)/user
DRIVER_DIR = $(ARCH_DIR)/drivers
BOOT_DIR = $(ARCH_DIR)/boot
LIBC_DIR = libc
ISO_DIR = iso

# Compiler and linker flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -O0 -I$(CURDIR)/include -std=gnu99
CFLAGS += -g  # Enable debugging
LDFLAGS = -nostdlib -T $(BOOT_DIR)/linker.ld
QEMU_FLAGS = -d int,page,cpu_reset,guest_errors -no-reboot -no-shutdown

# Files
KERNEL_BIN_ARCH = $(BUILD_DIR)/zineos.bin
KERNEL_BIN = $(ISO_DIR)/boot/zineos.bin
ISO_IMAGE = $(ISO_DIR)/zineos-$(ARCH).iso
DISK_IMAGE = $(ISO_DIR)/zdisk.img
LOG_FILE = serial_output.log

# Source files
KERNEL_SRC = $(shell find $(KERNEL_DIR) -name "*.c")
DRIVER_SRC = $(shell find $(DRIVER_DIR) -name "*.c")
ASM_SRC = $(shell find $(ARCH_DIR) -name "*.s")
FONT_PSF = resources/fonts/ter-powerline-v32n.psf

# Object files
KERNEL_OBJ = $(KERNEL_SRC:$(KERNEL_DIR)/%.c=$(BUILD_DIR)/kernel/%.o)
DRIVER_OBJ = $(DRIVER_SRC:$(DRIVER_DIR)/%.c=$(BUILD_DIR)/drivers/%.o)
ASM_OBJ = $(ASM_SRC:$(ARCH_DIR)/%.s=$(BUILD_DIR)/%.s.o)
FONT_OBJ = $(BUILD_DIR)/font.o
LIBC_OBJ = $(BUILD_DIR)/libc/libc.a
OBJS = $(KERNEL_OBJ) $(DRIVER_OBJ) $(LIBC_OBJ) $(ASM_OBJ) $(FONT_OBJ)

USER_BIN=$(wildcard $(BUILD_DIR)/user/bin/*)

# Build rules
.PHONY: all run debug clean help userland libc disk_image

all: libc userland $(ISO_IMAGE) disk_image

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/kernel $(BUILD_DIR)/drivers $(BUILD_DIR)/libc \
	         $(BUILD_DIR)/kernel/memory $(BUILD_DIR)/kernel/cpu $(BUILD_DIR)/kernel/lib \
			 $(BUILD_DIR)/kernel/gui $(BUILD_DIR)/kernel/fs $(BUILD_DIR)/kernel/scheduler \
			 $(BUILD_DIR)/boot

$(FONT_OBJ): $(FONT_PSF) | $(BUILD_DIR)
	@echo "Building font object..."
	i686-elf-objcopy -O elf32-i386 -B i386 -I binary $< $@

$(BUILD_DIR)/%.s.o: $(ARCH_DIR)/%.s | $(BUILD_DIR)
	@echo "Assembling $<..."
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling kernel source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/%.o: $(DRIVER_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling driver source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@

userland: $(BUILD_DIR)
	@echo "Compiling user source files $<..."
	$(MAKE) -C $(USER_DIR)

libc: $(BUILD_DIR)
	@echo "Compiling libc source files $<..."
	$(MAKE) -C $(LIBC_DIR)

$(KERNEL_BIN_ARCH): $(OBJS) $(BOOT_DIR)/linker.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN_ARCH) $(OBJS)

$(ISO_IMAGE): $(KERNEL_BIN_ARCH) $(DRIVER_OBJ)
	rm -f $(ISO_IMAGE)
	mkdir -p iso/boot/grub
	cp $(KERNEL_BIN_ARCH) $(KERNEL_BIN)
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

disk_image: $(DISK_IMAGE)
$(DISK_IMAGE):
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMAGE)

run: $(ISO_IMAGE) $(DISK_IMAGE)
	qemu-system-$(SCAMARCH) $(QEMU_FLAGS) \
		-cdrom $(ISO_IMAGE) \
		-drive file=$(DISK_IMAGE),format=raw \
		-serial file:$(LOG_FILE) \
		-boot d \
		-vga std

debug: $(ISO_IMAGE)
	qemu-system-$(SCAMARCH) -s -S \
		-cdrom $(ISO_IMAGE) \
		-drive file=$(DISK_IMAGE),format=raw \
		-serial file:$(LOG_FILE) \
		-boot d \
		-vga std &

	sleep 1
	$(GDB) -ex "target remote localhost:1234" -ex "symbol-file $(KERNEL_BIN_ARCH)"

clean:
	rm -rf $(BUILD_DIR) $(KERNEL_BIN_ARCH) $(ISO_IMAGE) $(KERNEL_BIN) $(LOG_FILE)

help:
	@echo "Available targets:"
	@echo "  make       - Build the kernel iso file"
	@echo "  run        - Run the kernel iso in QEMU"
	@echo "  debug      - Run the kernel iso in QEMU with GDB server"
	@echo "  clean      - Clean the build directory"

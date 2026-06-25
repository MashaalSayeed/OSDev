ARCH ?= i386
SCAMARCH ?= i386
TARGET = i686-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld
GDB = i386-elf-gdb
AS = nasm

PWD := $(shell pwd | sed 's/ /\\ /g')

# Directories
ARCH_DIR = arch/$(ARCH)
BUILD_DIR = build/$(ARCH)
KERNEL_DIR = kernel
USER_DIR = user
DRIVER_DIR = $(ARCH_DIR)/drivers
BOOT_DIR = $(ARCH_DIR)/boot
LIBC_DIR = libc
ISO_DIR = iso

# Compiler and linker flags
CFLAGS = -ffreestanding -Wall -Wextra -O0 -I$(CURDIR)/include -std=gnu99
# Keep debug symbols by default. Set KEEP_DEBUG=0 to build without -g.
KEEP_DEBUG ?= 1
ifeq ($(KEEP_DEBUG),1)
CFLAGS += -g
endif
LDFLAGS = -nostdlib -T $(BOOT_DIR)/linker.ld
QEMU_FLAGS = -d int,page,cpu_reset,guest_errors -no-reboot -no-shutdown
ASM_FLAGS = -f elf32

# Check if GUI is enabled
ifeq ($(GUI), 1)
	ASM_FLAGS += -D ENABLE_GUI
endif

# Files
KERNEL_BIN_ARCH = $(BUILD_DIR)/zineos.bin
KERNEL_BIN = $(ISO_DIR)/boot/zineos.bin
ISO_IMAGE = $(ISO_DIR)/zineos-$(ARCH).iso
DISK_IMAGE = $(ISO_DIR)/zdisk.img
LOG_FILE   = serial_output.log

GDB = i386-elf-gdb
KERNEL_BIN_ARCH = $(BUILD_DIR)/zineos.bin

.PHONY: all run debug clean help userland libc kernel disk_image

all: libc kernel userland disk_image

kernel: libc
	$(MAKE) -C kernel iso

libc:
	$(MAKE) -C libc

userland:
	$(MAKE) -C user
	@echo "Installing userland..."
	./scripts/install.sh

disk_image: $(DISK_IMAGE)
$(DISK_IMAGE):
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=64
	mkfs.fat -F 32 $(DISK_IMAGE)

run: all
	qemu-system-$(SCAMARCH) -d int,page,cpu_reset,guest_errors -no-reboot -no-shutdown \
		-cdrom $(ISO_IMAGE) \
		-drive file=$(DISK_IMAGE),format=raw \
		-serial file:$(LOG_FILE) \
		-boot d -vga std

debug: all
	qemu-system-$(SCAMARCH) -s -S \
		-cdrom $(ISO_IMAGE) \
		-drive file=$(DISK_IMAGE),format=raw \
		-serial file:$(LOG_FILE) \
		-boot d \
		-vga std & \
	QEMU_PID=$$!; \
	sleep 1; \
	$(GDB) \
		-ex "target remote localhost:1234" \
		-ex "set confirm off" \
		-ex "symbol-file $(KERNEL_BIN_ARCH)" \
		-ex "add-symbol-file $(BUILD_DIR)/user/bin/shell" 
		-ex "set confirm on"; \
	echo "GDB exited. Killing QEMU (PID: $$QEMU_PID)..."; \
	kill -9 $$QEMU_PID 2>/dev/null || true

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C libc clean
	$(MAKE) -C user clean
	rm -f $(DISK_IMAGE) $(LOG_FILE)

help:
	@echo "Targets: all  kernel  libc  userland  run  debug  clean"
ARCH     ?= i386
TARGET   = i686-elf
CC       = $(TARGET)-gcc
LD       = $(TARGET)-ld
AS       = nasm

# Directories
ARCH_DIR   = ../arch/$(ARCH)
BUILD_DIR  = ../build/$(ARCH)
DRIVER_DIR = $(ARCH_DIR)/drivers
BOOT_DIR   = $(ARCH_DIR)/boot
LIBC_DIR   = ../libc
ISO_DIR    = ../iso

FONT_PSF = ../resources/fonts/font.psf

# Flags
CFLAGS   = -ffreestanding -Wall -Wextra -O0 -I$(CURDIR)/../include -std=gnu99
KEEP_DEBUG ?= 1
ifeq ($(KEEP_DEBUG),1)
CFLAGS  += -g
endif
LDFLAGS  = -nostdlib -T $(BOOT_DIR)/linker.ld
ASM_FLAGS = -f elf32

ifeq ($(GUI),1)
ASM_FLAGS += -D ENABLE_GUI
endif

# Output
KERNEL_BIN_ARCH = $(BUILD_DIR)/zineos.bin
KERNEL_BIN      = $(ISO_DIR)/boot/zineos.bin
ISO_IMAGE       = $(ISO_DIR)/zineos-$(ARCH).iso

# Sources
KERNEL_SRC      = $(shell find . -name "*.c")
ARCH_KERNEL_SRC = $(shell find $(ARCH_DIR)/kernel -name "*.c")
DRIVER_SRC      = $(shell find $(DRIVER_DIR) -name "*.c")
ASM_SRC         = $(shell find $(ARCH_DIR)/kernel $(BOOT_DIR) -name "*.s")

# Objects
KERNEL_OBJ      = $(KERNEL_SRC:./%.c=$(BUILD_DIR)/kernel/%.o)
ARCH_KERNEL_OBJ = $(ARCH_KERNEL_SRC:$(ARCH_DIR)/kernel/%.c=$(BUILD_DIR)/kernel/%.o)
DRIVER_OBJ      = $(DRIVER_SRC:$(DRIVER_DIR)/%.c=$(BUILD_DIR)/drivers/%.o)
ASM_OBJ         = $(ASM_SRC:$(ARCH_DIR)/%.s=$(BUILD_DIR)/%.s.o)
FONT_OBJ        = $(BUILD_DIR)/font.o
LIBC_OBJ        = $(BUILD_DIR)/libc/libc.a

OBJS = $(KERNEL_OBJ) $(ARCH_KERNEL_OBJ) $(DRIVER_OBJ) $(LIBC_OBJ) $(ASM_OBJ) $(FONT_OBJ)

.PHONY: all iso clean

all: $(KERNEL_BIN_ARCH)

iso: $(ISO_IMAGE)

BUILD_SENTINEL = $(BUILD_DIR)/.build_dirs

$(BUILD_SENTINEL):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/kernel $(BUILD_DIR)/drivers $(BUILD_DIR)/libc \
	         $(BUILD_DIR)/kernel/memory $(BUILD_DIR)/kernel/cpu $(BUILD_DIR)/kernel/lib \
	         $(BUILD_DIR)/kernel/gui $(BUILD_DIR)/kernel/fs $(BUILD_DIR)/kernel/scheduler \
	         $(BUILD_DIR)/boot
	@touch $@

$(FONT_OBJ): $(FONT_PSF) | $(BUILD_SENTINEL)
	@echo "Building font object..."
	i686-elf-objcopy -O elf32-i386 -B i386 -I binary $< $@

$(BUILD_DIR)/boot/%.s.o: $(BOOT_DIR)/%.s | $(BUILD_SENTINEL)
	@echo "Assembling $<..."
	$(AS) $(ASM_FLAGS) $< -o $@

$(BUILD_DIR)/kernel/%.s.o: $(ARCH_DIR)/kernel/%.s | $(BUILD_SENTINEL)
	@echo "Assembling $<..."
	$(AS) $(ASM_FLAGS) $< -o $@

$(BUILD_DIR)/kernel/%.o: ./%.c | $(BUILD_SENTINEL)
	@echo "Compiling kernel source $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: $(ARCH_DIR)/kernel/%.c | $(BUILD_SENTINEL)
	@echo "Compiling arch-kernel source $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/%.o: $(DRIVER_DIR)/%.c | $(BUILD_SENTINEL)
	@echo "Compiling driver $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN_ARCH): $(OBJS) $(BOOT_DIR)/linker.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(ISO_IMAGE): $(KERNEL_BIN_ARCH)
	rm -f $(ISO_IMAGE)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN_ARCH) $(KERNEL_BIN)
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

clean:
	rm -rf $(BUILD_DIR) $(KERNEL_BIN_ARCH) $(ISO_IMAGE) $(KERNEL_BIN)
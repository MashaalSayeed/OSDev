ARCH?=i386
SCAMARCH?=i386
TARGET=i386-elf
CC=$(TARGET)-gcc
LD=$(TARGET)-ld
AS=$(TARGET)-as
GDB=$(TARGET)-gdb

PWD=${shell pwd | sed 's/ /\\ /g'}

# Compiler and linker flags
CFLAGS=-ffreestanding -O2 -Wall -Wextra -O0 -I${PWD}/include -std=gnu99
CFLAGS+=-g # Enable debugging
LDFLAGS=-nostdlib -T arch/$(ARCH)/linker.ld
QEMU_FLAGS=-d int,page,cpu_reset

BUILD_DIR=build/$(ARCH)
KERNEL_BIN=$(BUILD_DIR)/zineos.bin
ISO_IMAGE=iso/zineos-$(ARCH).iso

# Directories
ARCH_DIR=arch/$(ARCH)
KERNEL_DIR=$(ARCH_DIR)/kernel
DRIVERS_DIR=drivers/$(ARCH)
LIBC_DIR=libc

# Source files
BOOT_SRC=$(wildcard $(ARCH_DIR)/*.s)
KERNEL_SRC=$(shell find $(KERNEL_DIR) -name "*.c")
DRIVER_SRC=$(wildcard $(DRIVERS_DIR)/*.c)
LIBC_SRC=$(wildcard $(LIBC_DIR)/*.c)
FONT_PSF=resources/fonts/ter-powerline-v32n.psf

# Object files
KERNEL_OBJ=$(KERNEL_SRC:$(KERNEL_DIR)/%.c=$(BUILD_DIR)/kernel/%.o)
DRIVER_OBJ=$(DRIVER_SRC:$(DRIVERS_DIR)/%.c=$(BUILD_DIR)/drivers/%.o)
LIBC_OBJ=$(LIBC_SRC:$(LIBC_DIR)/%.c=$(BUILD_DIR)/libc/%.o)
ASM_OBJ=$(BOOT_SRC:$(ARCH_DIR)/%.s=$(BUILD_DIR)/%.o)
FONT_OBJ=$(BUILD_DIR)/font.o
OBJS=$(KERNEL_OBJ) $(DRIVER_OBJ) $(LIBC_OBJ) $(ASM_OBJ) $(FONT_OBJ)

# Build rules
all: $(ISO_IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR) $(BUILD_DIR)/kernel $(BUILD_DIR)/drivers $(BUILD_DIR)/libc \
	         $(BUILD_DIR)/kernel/memory $(BUILD_DIR)/kernel/cpu \
			 $(BUILD_DIR)/kernel/gui


# Building font objecet for i386 architecture
$(FONT_OBJ): $(FONT_PSF) | $(BUILD_DIR)
	@echo "Building font object..."
	i686-elf-objcopy -O elf32-i386 -B i386 -I binary $< $@

$(DRIVER_OBJ):
	make -C drivers/$(ARCH) ARCH=$(ARCH) CFLAGS="$(CFLAGS)" CC=${CC}

$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling libc source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(ARCH_DIR)/%.s | $(BUILD_DIR)
	@echo "Assembling $<..."
	nasm -f elf32 $< -o $@

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling kernel source file $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) $(ARCH_DIR)/linker.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJS)

$(ISO_IMAGE): $(KERNEL_BIN)
	rm $(ISO_IMAGE) || true
	mkdir -p iso/boot/grub
	cp $(KERNEL_BIN) iso/boot/zineos.bin
	grub-mkrescue -o $(ISO_IMAGE) iso

run: $(ISO_IMAGE)
# qemu-system-$(ARCH) -kernel $(KERNEL_BIN) -serial file:serial_output.log -vga std
	qemu-system-$(SCAMARCH) $(QEMU_FLAGS) $(ISO_IMAGE) -serial file:serial_output.log -vga std

debug: $(ISO_IMAGE)
# qemu-system-$(ARCH) -kernel $(KERNEL_BIN) -s -S &
	qemu-system-$(SCAMARCH) $(ISO_IMAGE) -s -S &
	sleep 1
	$(GDB) -ex "target remote localhost:1234" -ex "symbol-file $(KERNEL_BIN)"

clean:
	rm -rf $(BUILD_DIR) $(KERNEL_BIN) $(ISO_IMAGE) iso/boot/zineos.bin

help:
	@echo "Available targets:"
	@echo "  make       - Build the kernel iso file"
	@echo "  run        - Run the kernel iso in QEMU"
	@echo "  debug      - Run the kernel iso in QEMU with GDB server"
	@echo "  clean      - Clean the build directory"

# OSDev

## Build setup using brew on MacOS

1. First off, clone the repo:
```bash
git clone https://github.com/MashaalSayeed/OSDev
cd OSDev
```
2. Run setup.sh to install all dependencies
```bash
chmod +x scripts/setup.sh
scripts/setup.sh
```

3. Run build.sh to build the OS and build grub
```bash
chmod +x scripts/build.sh
scripts/build.sh
```

4. Create a harddisk for your system
```bash
dd if=/dev/zero of=zdisk.img bs=1M count=64
mkfs.fat -F 32 zdisk.img
```

5. Boot the generated ISO file
```bash
qemu-system-i386 -cdrom iso/zineos-i686.iso -hda iso/zdisk.img
```
Or, you have a nicer way:
```bash
make run
```

Thank me later 😊

## TODO List Guide

### 1. Basic

1. Boot Kernel ✅
2. Setup GDT ✅
4. Setup IDT ✅
5. Timer ✅
6. Higher Half Kernel ✅
7. Generate Bootable ISO ✅
8. Shift To Multiboot2 ✅
9. Process Scheduling ✅
10. Virtual File System ✅
11. Multithreading
12. Userspace Mode (Ring 3)
13. Shell

### 2. Memory Management

1. Physical Memory Manager ✅
2. Virtual Memory Manager ✅
3. Paging Setup ✅
4. Heap Allocation ✅

### 3. File System

1. Virtual File System ✅
2. FAT32 System ✅
3. Subdirectory Support
4. Long Filenames Support

### 4. Drivers

1. VGA Driver ✅
2. Serial Driver ✅
3. PIT (Programmable Interrupt Timer) ✅
4. PS2 Keyboard Driver ✅
5. VBE Video ✅
6. ATA Driver ✅
7. PCI Driver ✅
8. Mouse Driver
9. RTC (Real Time Clock)
10. Network Drivers

### 5. Video Output
1. VBE Driver ✅
2. Draw Images ✅
3. Draw Fonts ✅
4. Window Manager

### 6. Others

1. Libc string functions ✅
2. Basic printf debugging ✅
3. ACPI ✅
4. APIC
5. Exception Handling
6. Kernel Panics
7. ELF Loader
8. Test Cases

## Resources

1. https://wiki.osdev.org/
2. http://www.osdever.net/tutorials/
3. https://github.com/dreamportdev/Osdev-Notes
4. https://github.com/szhou42/osdev
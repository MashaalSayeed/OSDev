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
./scripts/setup.sh
```

3. Run build.sh to build the OS and build grub
```bash
chmod +x scripts/build.sh
./scripts/build.sh
```

4. Install the user binaries onto the virtual disk
```bash
./scripts/install.sh
```

5. Boot the generated ISO file
```bash
make run
```

or for GUI
```bash
make run GUI=1
```

Thank me later 😊

## Folder Structure Guide

-- TODO

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
11. Userspace Mode (Ring 3) ✅
12. Shell ✅
13. Syscalls ✅
14. Multithreading 

### 2. Memory Management

1. Physical Memory Manager ✅
2. Virtual Memory Manager ✅
3. Paging Setup ✅
4. Heap Allocation ✅
5. Slab Allocator

### 3. Process Scheduling

1. Round Robin Scheduler ✅
2. Process heap & stack ✅
3. `fork()` / `exec()` 🟡
4. Threads
5. Signals and IPC

### 4. File System

1. Virtual File System ✅
2. FAT32 System ✅
3. Subdirectory Support ✅
4. Load ELF Binaries ✅
5. Long Filenames Support 🟡
6. EXT2 ?

### 5. Drivers

1. VGA Driver ✅
2. Serial Driver ✅
3. PIT (Programmable Interrupt Timer) ✅
4. PS2 Keyboard Driver ✅
5. VBE Video ✅
6. ATA Driver ✅
7. RTC (Real Time Clock) ✅
8. PCI Driver
9. Mouse Driver
10. Network Drivers

### 6. Video Output

1. VBE Driver ✅
2. Draw Images ✅
3. Draw Fonts ✅
4. Window Manager 🟡

### 7. Shell

1. Basic Commands Handling ✅
2. Load and Execute ELFs ✅
3. Movement with arrow keys 🟡
4. Piping and Redirection 🟡
5. Text Editor

### 8. Others

1. Basic printf debugging ✅
3. Exception Handling ✅
4. Test Cases 🟡
5. Make a [Complete ISO](https://wiki.osdev.org/GRUB#Disk_image_instructions) (GRUB + Kernel + Disk Image)
6. Build setup on other platforms 
7. Fix the folder structure

## Resources

1. https://wiki.osdev.org/
2. https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html
3. http://www.osdever.net/tutorials/
4. https://github.com/dreamportdev/Osdev-Notes
5. https://github.com/szhou42/osdev
6. https://forum.osdev.org/viewtopic.php?t=30577

## Known Issues

1. Random page faults when using fork()/exec() method to spawn a process
#pragma once

#include <stdint.h>

// Kernel heap size = 48MB
#define KHEAP_START 0xC0400000
#define KHEAP_INITIAL_SIZE 48 * 0x100000
#define KHEAP_MIN_SIZE 0x100000
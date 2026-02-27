#pragma once

/*
 * shm.h — Kernel shared-memory subsystem.
 *
 * Allows user processes to share physical pages without copying.
 * The compositor uses this to give each client window a pixel buffer that
 * both parties can access through their own virtual addresses.
 *
 * Layout:
 *   Kernel VA window:  SHM_KERNEL_VBASE + slot * SHM_SLOT_SIZE
 *   User   VA window:  SHM_USER_VBASE   + slot * SHM_SLOT_SIZE
 *
 * The compositor maps each window's SHM via the kernel window; clients
 * map the same SHM object via the user window (same physical frames,
 * different virtual addresses in their own page directories).
 */

#include <stdint.h>
#include "kernel/paging.h"

/* Maximum number of live SHM objects */
#define SHM_MAX_OBJECTS   64

/*
 * Virtual-address slots.
 * Each slot is 4 MB wide, which fits a 1024×768×4-byte framebuffer (3 MB).
 *
 * Kernel backing:  0xC3400000 .. 0xD3400000  (just above the 48 MB heap)
 * User mapping:    0x80000000 .. 0x90000000  (well above heap, below stacks)
 */
#define SHM_SLOT_SIZE     0x400000u         /* 4 MB per slot              */
#define SHM_KERNEL_VBASE  0xC3400000u       /* kernel VA for SHM backing  */
#define SHM_USER_VBASE    0x80000000u       /* user   VA for SHM mappings */

typedef struct {
    int      id;        /* unique handle returned to callers             */
    int      slot;      /* index into shm_table (used for VA arithmetic) */
    uint32_t kvaddr;    /* kernel virtual address of the backing mapping */
    uint32_t size;      /* byte size, page-aligned                       */
    int      ref_count; /* number of active shm_map() calls              */
    int      in_use;    /* 1 if this slot is allocated                   */
} shm_object_t;

/* Initialise the SHM table – call once during kernel startup */
void          shm_init(void);

/* Allocate SHM backed by fresh physical pages; returns shm_id or -1 */
int           shm_create(uint32_t size);

/*
 * Map SHM object into the *current* process's address space.
 * Returns the user virtual address, or NULL on failure.
 * Increments ref_count.
 */
void         *shm_map(int shm_id);

/*
 * Unmap SHM from the *current* process's address space.
 * Decrements ref_count; does NOT free physical pages (use shm_destroy).
 */
int           shm_unmap(int shm_id);

/*
 * Free the physical pages backing an SHM object.
 * Fails (returns -1) if ref_count > 0 (still mapped somewhere).
 */
int           shm_destroy(int shm_id);

/* Internal: look up an object by ID */
shm_object_t *shm_get(int shm_id);

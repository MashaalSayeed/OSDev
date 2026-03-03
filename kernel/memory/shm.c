/*
 * shm.c — Kernel shared-memory subsystem.
 *
 * How it works
 * ============
 * 1. shm_create(size)
 *      Allocates 'size' (rounded up to pages) in the kernel's virtual
 *      address window (SHM_KERNEL_VBASE + slot*SHM_SLOT_SIZE) by calling
 *      kmap_memory(..., physical=0, ...) which auto-allocates physical frames
 *      via the PMM.  Returns a small integer shm_id.
 *
 * 2. shm_map(shm_id)   -- called by compositor AND by client processes
 *      For each physical page in the SHM object (retrieved via
 *      virtual2physical on the kernel backing VA), installs the same frame
 *      number into the calling process's page directory at
 *      SHM_USER_VBASE + slot*SHM_SLOT_SIZE + page_offset.
 *      Returns the user virtual address.
 *
 * 3. shm_unmap(shm_id)
 *      Clears the PTEs in the calling process's page directory (does NOT
 *      free the physical frames — the kernel still owns them).
 *      Flushes the TLB by reloading CR3.
 *
 * 4. shm_destroy(shm_id)
 *      Frees the physical pages (via free_page on the kernel-side PTEs).
 *      Only allowed when ref_count == 0.
 */

#include "kernel/shm.h"
#include "kernel/paging.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "kernel/process.h"
#include "libc/string.h"

extern page_directory_t *kpage_dir;

static shm_object_t shm_table[SHM_MAX_OBJECTS];
static int          shm_next_id = 1;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void shm_init(void) {
    memset(shm_table, 0, sizeof(shm_table));
    kprintf(DEBUG, "shm: initialized (%d slots, user base %x)\n",
            SHM_MAX_OBJECTS, SHM_USER_VBASE);
}

int shm_create(uint32_t size) {
    if (size == 0) return -1;

    /* Round up to page boundary */
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Find a free table slot */
    for (int i = 0; i < SHM_MAX_OBJECTS; i++) {
        if (shm_table[i].in_use) continue;

        uint32_t kvaddr = SHM_KERNEL_VBASE + (uint32_t)i * SHM_SLOT_SIZE;

        /*
         * Map into kernel virtual space with auto-allocation (physical=0).
         * This calls alloc_page() for every page, pulling frames from PMM.
         * The kernel will be able to read/write the buffer at kvaddr.
         */
        kmap_memory(kvaddr, -1, size, PAGE_PRESENT | PAGE_RW | PAGE_USER);

        shm_table[i].id        = shm_next_id++;
        shm_table[i].slot      = i;
        shm_table[i].kvaddr    = kvaddr;
        shm_table[i].size      = size;
        shm_table[i].ref_count = 0;
        shm_table[i].in_use    = 1;

        kprintf(DEBUG, "shm_create: id=%d slot=%d kvaddr=%x size=%d\n",
                shm_table[i].id, i, kvaddr, size);
        return shm_table[i].id;
    }

    kprintf(ERROR, "shm_create: table full\n");
    return -1;
}

void *shm_map(int shm_id) {
    /* Find the SHM object */
    shm_object_t *obj = shm_get(shm_id);
    if (!obj) {
        kprintf(ERROR, "shm_map: unknown id %d\n", shm_id);
        return NULL;
    }

    process_t  *proc      = get_current_process();
    uint32_t    user_base = SHM_USER_VBASE + (uint32_t)obj->slot * SHM_SLOT_SIZE;

    /*
     * For every page in the SHM object, look up its physical frame via the
     * kernel mapping, then install the *same* frame into the user process's
     * page directory at the corresponding user virtual address.
     */
    for (uint32_t off = 0; off < obj->size; off += PAGE_SIZE) {
        uint32_t phys = (uint32_t)virtual2physical(
                            kpage_dir, (void *)(obj->kvaddr + off));
        if (!phys) {
            kprintf(ERROR, "shm_map: virtual2physical failed at kvaddr+%x\n", off);
            return NULL;
        }

        page_table_entry_t *pte = get_page(user_base + off, 1,
                                           proc->root_page_table);
        if (!pte) {
            kprintf(ERROR, "shm_map: get_page failed for uva=%x\n",
                    user_base + off);
            return NULL;
        }

        if (pte->present) {
            /* Already mapped (e.g. compositor mapped it first) – skip */
            continue;
        }

        pte->frame   = phys >> 12;
        pte->present = 1;
        pte->rw      = 1;
        pte->user    = 1;
    }

    obj->ref_count++;
    kprintf(DEBUG, "shm_map: id=%d -> uva=%x (pid=%d, ref=%d)\n",
            shm_id, user_base, proc->pid, obj->ref_count);
    return (void *)user_base;
}

int shm_unmap(int shm_id) {
    shm_object_t *obj = shm_get(shm_id);
    if (!obj) return -1;

    process_t  *proc      = get_current_process();
    uint32_t    user_base = SHM_USER_VBASE + (uint32_t)obj->slot * SHM_SLOT_SIZE;

    for (uint32_t off = 0; off < obj->size; off += PAGE_SIZE) {
        page_table_entry_t *pte = get_page(user_base + off, 0,
                                           proc->root_page_table);
        if (pte && pte->present) {
            /*
             * Clear the PTE but do NOT call free_page() – the physical frame
             * is owned by the SHM object, not by this process.
             */
            pte->present = 0;
            pte->rw      = 0;
            pte->user    = 0;
            pte->frame   = 0;
        }
    }

    if (obj->ref_count > 0) obj->ref_count--;

    /* Flush TLB by reloading CR3 */
    asm volatile("movl %%cr3, %%eax; movl %%eax, %%cr3" ::: "eax", "memory");

    kprintf(DEBUG, "shm_unmap: id=%d (pid=%d, ref=%d)\n",
            shm_id, proc->pid, obj->ref_count);
    return 0;
}

int shm_destroy(int shm_id) {
    shm_object_t *obj = shm_get(shm_id);
    if (!obj) return -1;

    if (obj->ref_count > 0) {
        kprintf(WARNING, "shm_destroy: id=%d still has %d mappings\n",
                shm_id, obj->ref_count);
        return -1;
    }

    /* Free each physical frame via the kernel-side PTEs */
    for (uint32_t off = 0; off < obj->size; off += PAGE_SIZE) {
        page_table_entry_t *pte = get_page(obj->kvaddr + off, 0, kpage_dir);
        if (pte && pte->present) {
            free_page(pte);  /* clears present, rw, user, frame and PMM bit */
        }
    }

    kprintf(DEBUG, "shm_destroy: id=%d freed %d bytes\n", shm_id, obj->size);
    memset(obj, 0, sizeof(*obj));
    return 0;
}

shm_object_t *shm_get(int shm_id) {
    for (int i = 0; i < SHM_MAX_OBJECTS; i++) {
        if (shm_table[i].in_use && shm_table[i].id == shm_id)
            return &shm_table[i];
    }
    return NULL;
}

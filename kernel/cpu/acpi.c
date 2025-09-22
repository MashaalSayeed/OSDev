#include "kernel/acpi.h"
#include "libc/string.h"
#include "kernel/printf.h"
#include "kernel/paging.h"

static rsdp_descriptor_t *rsdp = NULL;
static acpi_table_header_t *rsdt = NULL;

uint8_t calculate_checksum(void *table, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += ((uint8_t*)table)[i];
    }
    return sum == 0;
}

void *find_rsdp() {
    // Scan the memory range 0x000E0000 to 0x00100000 for the RSDP signature
    uint32_t addr = 0xC00E0000; 
    uint32_t end =  0xC0100000;

    for (; addr < end; addr += 16) {
        if (!memcmp((void*)addr, RSDP_SIGNATURE, 8)) {
            return (void*)addr;
        }
    }
    return NULL;
}

acpi_table_header_t *find_table(acpi_table_header_t *table, char *signature) {
    uint32_t entries = (table->length - sizeof(acpi_table_header_t)) / sizeof(uint32_t);
    for (uint32_t i = 0; i < entries; i++) {
        acpi_table_header_t *header = (acpi_table_header_t*)(table + sizeof(acpi_table_header_t) + i * sizeof(uint32_t));
        if (!memcmp(header->signature, signature, 4)) {
            return header;
        }
    }
    return NULL;
}

void find_rsdt() {
    if (!rsdt) {
        printf("RSDT not found\n");
        return;
    }

    acpi_table_header_t* rsdt_virt = 0xC0000000 + rsdt;
    // map_physical_to_virtual((uint32_t) rsdt_virt, (uint32_t) rsdt);
    kmap_memory((uint32_t)rsdt_virt, (uint32_t)rsdt, sizeof(acpi_table_header_t), 0x7);
    uint32_t num_entries = (rsdt_virt->length - sizeof(acpi_table_header_t)) / sizeof(uint32_t);

    printf("RSDT found at %x\n", rsdt_virt);
    printf("Signature: %s\n", rsdt_virt->signature);
    printf("Length: %d\n\n", rsdt_virt->length);
    printf("OEM ID: %s\n", rsdt_virt->oem_id);
    printf("RSDT has %d entries\n", num_entries);

    // TODO: Iterate through the entries to find other tables
}

void acpi_init() {
    // RSDP (Root System Description Pointer) used in the ACPI programming interface is the pointer to the
    // RSDT (Root System Descriptor Table) which contains the description of the hardware components
    rsdp = (rsdp_descriptor_t *)find_rsdp();
    if (!rsdp) {
        printf("RSDP not found\n");
        return;
    }

    printf("RSDP found at %x\n", rsdp);
    printf("OEM ID: %s\n", rsdp->oem_id);
    printf("Revision: %d\n", rsdp->revision);
    printf("RSDT Address: %x\n\n", rsdp->rsdt_address);

    rsdt = (acpi_table_header_t*)rsdp->rsdt_address;
    // find_rsdt();
}
#pragma once

#include <stdint.h>
#include <stddef.h>

#define RSDP_SIGNATURE "RSD PTR "

typedef struct rsdp_descriptor {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_descriptor_t;

typedef struct acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_table_header_t;


uint8_t calculate_checksum(void *table, size_t length);
void *find_rsdp();
acpi_table_header_t *find_table(acpi_table_header_t *table, char *signature);
void find_rsdt();
void acpi_init();

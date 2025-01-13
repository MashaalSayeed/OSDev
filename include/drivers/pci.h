#pragma once

#include <stdint.h>

typedef struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
} pci_device_t;

pci_device_t* pci_get_device(uint16_t vendor_id, uint16_t device_id);
uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
int pci_scan(int max_devices);
void pci_init();
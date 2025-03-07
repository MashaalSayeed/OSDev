#pragma once

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

typedef struct {
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
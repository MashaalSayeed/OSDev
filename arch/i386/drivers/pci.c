#include <stddef.h>

#include "drivers/pci.h"
#include "kernel/printf.h"
#include "kernel/io.h"

pci_device_t devices[32];

pci_device_t* pci_get_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < 32; i++) {
        if (devices[i].vendor_id == vendor_id && devices[i].device_id == device_id) {
            return &devices[i];
        }
    }

    return NULL;
}

uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (function << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

int pci_scan(int max_devices) {
    int num_devices = 0;
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int function = 0; function < 8; function++) {
                uint32_t id = pci_read(bus, slot, function, 0);
                if (id == 0xFFFFFFFF) continue;

                devices[num_devices].bus = bus;
                devices[num_devices].slot = slot;
                devices[num_devices].function = function;
                devices[num_devices].vendor_id = id & 0xFFFF;
                devices[num_devices].device_id = id >> 16;

                // uint32_t out = pci_read(bus, slot, function, 0x08);
                // uint8_t class_id = out >> 24;
                // uint8_t subclass_id = (out >> 16) & 0xFF;
                // uint8_t prog_if = (out >> 8) & 0xFF;

                // printf("PCI Device: %x:%x:%x | Vendor: %x | Device: %x | Class: %x | Subclass: %x | Prog IF: %x\n",
                //     bus, slot, function, devices[num_devices].vendor_id, devices[num_devices].device_id, class_id, subclass_id, prog_if);
                num_devices++;
                if (num_devices >= max_devices) return num_devices;
            }
        }
    }

    return num_devices;
}

void pci_init() {
    // Initialize the PCI devices
    int num_devices = pci_scan(32);
    printf("PCI Devices: %d\n", num_devices);
}
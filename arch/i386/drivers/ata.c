#include "drivers/ata.h"
#include "drivers/pci.h"
#include "kernel/isr.h"
#include "kernel/vfs.h"
#include "kernel/kheap.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "io.h"

#define ATA_TIMEOUT 1000000

int ata_wait_busy(uint16_t io_base) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if ((status & ATA_STATUS_BSY) == 0) return 0;
    }
    return -1;
}

int ata_wait_drq(uint16_t io_base) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_STATUS_DRQ) return 0;
    }
    return -1;
}

void ata_irq_handler(registers_t *regs) {
    uint8_t status = inb(ATA_PRIMARY_BASE_PORT + ATA_REG_STATUS);
    if (status & ATA_STATUS_ERR) {
        printf("ATA Error: %x\n", inb(ATA_PRIMARY_BASE_PORT + ATA_REG_ERROR));
        return;
    }

    if (status & ATA_STATUS_DRQ) {
        // printf("ATA IRQ: Data ready\n");
        // TODO: Read the data if a read operation is in progress.
        // ata_read_sector_interrupt();
    } else {
        printf("ATA IRQ: No data ready\n");
    }

    // Acknowledge the interrupt
    inb(ATA_PRIMARY_BASE_PORT + ATA_REG_STATUS);
}

int ata_identify(uint16_t io_base, uint8_t drive) {
    if (ata_wait_busy(io_base) != 0) return -1;
    outb(io_base + ATA_REG_DEVICE, 0xA0 | (drive << 4));

    outb(io_base + ATA_REG_SECCOUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    if (inb(io_base + ATA_REG_STATUS) == 0) return -1; // Drive does not exist
    ata_wait_busy(io_base);

    if (inb(io_base + ATA_REG_STATUS) & ATA_STATUS_ERR) return -1; // Error identifying drive
    ata_wait_drq(io_base);

    ata_identify_t *id = (ata_identify_t*) kmalloc(sizeof(ata_identify_t));
    for (int i = 0; i < 256; i++) {
        ((uint16_t*)id)[i] = inw(io_base + ATA_REG_DATA);
    }

    uint8_t * ptr = (uint8_t *)id->model_number;
    for (int i = 0; i < 40; i += 2) {
        char tmp = ptr[i];
        ptr[i] = ptr[i + 1];
        ptr[i + 1] = tmp;
    }
    ptr[40] = '\0';
    printf("ATA Device: %s\n", id->model_number);
    return ATA_SUCCESS;
}

int ata_read_sector(uint16_t io_base, uint8_t drive, uint32_t lba, void *buffer) {
    outb(io_base + ATA_REG_DEVICE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(io_base + ATA_REG_SECCOUNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_READ);

    if (ata_wait_busy(io_base)) return ATA_ERR_BUSY;
    if (ata_wait_drq(io_base)) return ATA_ERR_DRQ;

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buffer)[i] = inw(io_base + ATA_REG_DATA);
    }

    if (inb(io_base + ATA_REG_STATUS) & ATA_STATUS_ERR) return ATA_ERR_READ;
    return ATA_SUCCESS;
}

int ata_write_sector(uint16_t io_base, uint8_t drive, uint32_t lba, void *buffer) {
    outb(io_base + ATA_REG_DEVICE, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(io_base + ATA_REG_SECCOUNT, 1);
    outb(io_base + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_WRITE);

    if (ata_wait_busy(io_base)) return ATA_ERR_BUSY;
    if (ata_wait_drq(io_base)) return ATA_ERR_DRQ;

    for (int i = 0; i < 256; i++) {
        outw(io_base + ATA_REG_DATA, ((uint16_t*)buffer)[i]);
    }

    outb(io_base + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    if (ata_wait_busy(io_base)) return ATA_ERR_BUSY;

    return ATA_SUCCESS;
}

int ata_read_block(block_device_t *dev, uint32_t block, void *buf) {
    return ata_read_sector(dev->device_data, ATA_MASTER, block, buf);
}

int ata_write_block(block_device_t *dev, uint32_t block, const void *buf) {
    return ata_write_sector(dev->device_data, ATA_MASTER, block, buf);
}

block_device_t ata = {
    .name = "/dev/sda1",
    .read_block = ata_read_block,
    .write_block = ata_write_block,
    .device_data = ATA_PRIMARY_BASE_PORT
};

void ata_init() {
    // Initialize the ATA devices
    if (ata_identify(ATA_PRIMARY_BASE_PORT, ATA_MASTER) != ATA_SUCCESS) {
        printf("No primary master ATA device found\n");
    } else {
        // uint32_t lba = 10;
        // // ata_write_sector(ATA_PRIMARY_BASE_PORT, ATA_MASTER, lba, "Hello, World!");
        // char buffer[512];
        // ata_read_sector(ATA_PRIMARY_BASE_PORT, ATA_MASTER, lba, buffer);
        // printf("Read from sector: %s\n", buffer);

        register_block_device(&ata);
    }

    // if (ata_identify(ATA_PRIMARY_BASE_PORT, ATA_SLAVE) != ATA_SUCCESS) {
    //     printf("No primary slave ATA device found\n");
    // }

    // if (ata_identify(ATA_SECONDARY_BASE_PORT, ATA_MASTER) != ATA_SUCCESS) {
    //     printf("No secondary master ATA device found\n");
    // }

    // if (ata_identify(ATA_SECONDARY_BASE_PORT, ATA_SLAVE) != ATA_SUCCESS) {
    //     printf("No secondary slave ATA device found\n");
    // }

    register_interrupt_handler(32 + 14, ata_irq_handler);

    // printf("ATA Initialized\n");
}
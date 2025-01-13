#pragma once

#include <stdint.h>

#define ATA_VENDOR_ID 0x8086
#define ATA_DEVICE_ID 0x7010

#define ATA_PRIMARY_BASE_PORT 0x1F0
#define ATA_SECONDARY_BASE_PORT 0x170
#define ATA_PRIMARY_CONTROL_PORT 0x3F6
#define ATA_SECONDARY_CONTROL_PORT 0x376

#define ATA_TYPE_ATA 0
#define ATA_TYPE_ATAPI 1

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT 0x02
#define ATA_REG_LBA_LOW 0x03
#define ATA_REG_LBA_MID 0x04
#define ATA_REG_LBA_HIGH 0x05
#define ATA_REG_DEVICE 0x06
#define ATA_REG_STATUS 0x07
#define ATA_REG_COMMAND 0x07

#define ATA_CMD_IDENTIFY  0xEC
#define ATA_CMD_READ      0x20
#define ATA_CMD_WRITE     0x30
#define ATA_CMD_FLUSH     0xE7

#define ATA_STATUS_ERR    0x01
#define ATA_STATUS_DRQ    0x08
#define ATA_STATUS_SRV    0x10
#define ATA_STATUS_DF     0x20
#define ATA_STATUS_RDY    0x40
#define ATA_STATUS_BSY    0x80

#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

typedef enum {
    ATA_SUCCESS = 0,
    ATA_ERR_BUSY = -1,
    ATA_ERR_DRQ = -2,
    ATA_ERR_READ = -3
} ata_status_t;

typedef struct {
    uint16_t general_config;           // Word 0: General configuration
    uint16_t reserved_1;               // Word 1: Reserved
    uint16_t specific_config;          // Word 2: Specific configuration
    uint16_t reserved_3;               // Word 3: Reserved
    uint16_t retired_4_5[2];           // Words 4-5: Retired
    uint16_t obsolete_6;               // Word 6: Obsolete
    uint16_t reserved_7_8[2];          // Words 7-8: Reserved
    uint16_t retired_9;                // Word 9: Retired
    char serial_number[20];            // Words 10-19: Serial number (ASCII)
    uint16_t retired_20_21[2];         // Words 20-21: Retired
    uint16_t obsolete_22;              // Word 22: Obsolete
    char firmware_revision[8];         // Words 23-26: Firmware revision (ASCII)
    char model_number[40];             // Words 27-46: Model number (ASCII)
    uint16_t max_sectors_per_interrupt; // Word 47: Maximum sectors per interrupt
    uint16_t reserved_48;              // Word 48: Reserved
    uint16_t capabilities_49_50[2];    // Words 49-50: Capabilities
    uint16_t obsolete_51;              // Word 51: Obsolete
    uint16_t pio_timing;               // Word 52: PIO data transfer cycle timing
    uint16_t dma_timing;               // Word 53: DMA data transfer cycle timing
    uint16_t field_validity;           // Word 54: Field validity
    uint16_t current_cylinders;        // Word 55: Current number of cylinders
    uint16_t current_heads;            // Word 56: Current number of heads
    uint16_t current_sectors;          // Word 57: Current sectors per track
    uint16_t current_capacity[2];      // Words 58-59: Current capacity in sectors
    uint16_t total_lba28_sectors[2];   // Words 60-61: Total number of LBA28 sectors
    uint16_t singleword_dma;           // Word 62: Single word DMA modes
    uint16_t multiword_dma;            // Word 63: Multiword DMA modes
    uint16_t pio_modes;                // Word 64: PIO modes supported
    uint16_t min_dma_cycle_time;       // Word 65: Minimum DMA cycle time
    uint16_t recommended_dma_cycle_time; // Word 66: Recommended DMA cycle time
    uint16_t min_pio_cycle_time;       // Word 67: Minimum PIO cycle time
    uint16_t min_pio_cycle_time_iordy; // Word 68: Minimum PIO cycle time with IORDY
    uint16_t reserved_69_79[11];       // Words 69-79: Reserved
    uint16_t major_version;            // Word 80: Major version number
    uint16_t minor_version;            // Word 81: Minor version number
    uint16_t command_set_82_87[6];     // Words 82-87: Command sets supported
    uint16_t command_set_88_91[4];     // Words 88-91: Command sets enabled
    uint16_t ultra_dma_mode;           // Word 93: Ultra DMA mode
    uint16_t reserved_94_127[34];      // Words 94-127: Reserved
    uint16_t security_status;          // Word 128: Security status
    uint16_t reserved_129_159[31];     // Words 129-159: Reserved
    uint16_t cfa_power_mode;           // Word 160: CFA power mode
    uint16_t reserved_161_175[15];     // Words 161-175: Reserved
    uint16_t media_serial_number[30];  // Words 176-205: Media serial number
    uint16_t reserved_206_254[49];     // Words 206-254: Reserved
    uint16_t integrity_word;           // Word 255: Integrity word
} __attribute__((packed)) ata_identify_t;


void ata_init();
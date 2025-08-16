#include "kernel/vfs.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include "kernel/bmp.h"

uint32_t *load_bitmap_file(const char* path, int *width, int *height) {
    vfs_file_t *file = vfs_open(path, VFS_FLAG_READ);
    if (!file) {
        printf("Error: Could not open bitmap file %s\n", path);
        return NULL;
    }

    bmp_header_t bmp_header;
    bmp_info_header_t bmp_info_header;

    if (vfs_read(file, &bmp_header, sizeof(bmp_header)) != sizeof(bmp_header)) {
        kprintf(WARNING, "Error: Could not read BMP header from %s\n", path);
        vfs_close(file);
        return NULL;
    }

    if (vfs_read(file, &bmp_info_header, sizeof(bmp_info_header)) != sizeof(bmp_info_header)) {
        kprintf(WARNING, "Error: Could not read BMP info header from %s\n", path);
        vfs_close(file);
        return NULL;
    }

    if (bmp_header.bfType != 0x4D42) { // 'BM'
        kprintf(WARNING, "Error: Not a valid BMP file\n");
        vfs_close(file);
        return NULL;
    }

    *width = bmp_info_header.biWidth;
    *height = bmp_info_header.biHeight;

    uint32_t *image_data = (uint32_t *)kmalloc(*width * *height * sizeof(uint32_t));
    size_t row_size = (*width * bmp_info_header.biBitCount / 8 + 3) & ~3; // Row size must be a multiple of 4 bytes
    size_t data_size = row_size * *height;

    uint8_t *pixel_buffer = (uint8_t *)kmalloc(data_size);
    vfs_seek(file, bmp_header.bfOffBits, VFS_SEEK_SET);
    vfs_read(file, pixel_buffer, data_size);
    vfs_close(file);

    for (int y = 0; y < *height; y++) {
        uint8_t *row = pixel_buffer + (row_size * y);
        for (int x = 0; x < *width; x++) {
            size_t pixel_offset = (x * (bmp_info_header.biBitCount / 8));
            uint8_t blue = row[pixel_offset];
            uint8_t green = row[pixel_offset + 1];
            uint8_t red = row[pixel_offset + 2];

            image_data[(*height - 1 - y) * (*width) + x] = (red << 16) | (green << 8) | blue; // BMP stores pixels bottom-up
        }
    }

    kfree(pixel_buffer);
    return image_data;
}
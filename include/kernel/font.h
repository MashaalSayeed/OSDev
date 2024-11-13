#pragma once

#include <stdint.h>

#define PSF_FONT_MAGIC 0x864ab572

typedef struct psf1_header {
    uint16_t magic;
    uint8_t mode;
    uint8_t charsize;
} psf1_header_t;

typedef struct psf_font {
    uint32_t magic;         /* magic bytes to identify PSF */
    uint32_t version;       /* zero */
    uint32_t headersize;    /* offset of bitmaps in file, 32 */
    uint32_t flags;         /* 0 if there's no unicode table */
    uint32_t numglyph;      /* number of glyphs */
    uint32_t bytesperglyph; /* size of each glyph */
    uint32_t height;        /* height in pixels */
    uint32_t width;         /* width in pixels */
} psf_font_t;

psf_font_t *load_psf_font();
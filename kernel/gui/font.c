#include "kernel/font.h"
#include "kernel/printf.h"
#include "kernel/kheap.h"
#include <limits.h>

extern uint8_t *_binary_resources_fonts_font_psf_start;
extern uint8_t *_binary_resources_fonts_font_psf_end;
extern psf_font_t *current_font;

uint16_t *unicode;

psf_font_t *load_psf_font() {
    psf_font_t *font = (psf_font_t *)&_binary_resources_fonts_font_psf_start;
    current_font = font;

    if (font->magic != 0x864AB572) {
        printf("Error: Invalid PSF font magic number\n");
        return NULL;
    }
    if (font->version != 0) {
        printf("Error: Unsupported PSF font version %d\n", font->version);
        return NULL;
    }

    printf("Font Num Glyphs %d\n", font->numglyph);
    printf("Font Header Size: %d\n", font->headersize);
    printf("Font Glyph Size %d\n", font->bytesperglyph);
    printf("Font Glyph Height x Width (%d x %d)\n\n", font->height, font->width);

    // Get the offset of the table
    uint8_t *s = (uint8_t *)(
        (unsigned char*)font +
        font->headersize +
        font->numglyph * font->bytesperglyph
    );

    unicode = calloc(USHRT_MAX, 2);
    uint16_t glyph = 0;

    while(s < _binary_resources_fonts_font_psf_end) {
        uint16_t uc = (uint16_t)(*(unsigned char *)s);
        if(uc == 0xFF) {
            glyph++;
            s++;
            continue;
        } else if(uc & 128) {
            /* UTF-8 to unicode */
            if((uc & 32) == 0 ) {
                uc = ((s[0] & 0x1F)<<6)+(s[1] & 0x3F);
                s++;
            } else
            if((uc & 16) == 0 ) {
                uc = ((((s[0] & 0xF)<<6)+(s[1] & 0x3F))<<6)+(s[2] & 0x3F);
                s+=2;
            } else
            if((uc & 8) == 0 ) {
                uc = ((((((s[0] & 0x7)<<6)+(s[1] & 0x3F))<<6)+(s[2] & 0x3F))<<6)+(s[3] & 0x3F);
                s+=3;
            } else
                uc = 0;
        }
        /* save translation */
        unicode[uc] = glyph;
        s++;
    }

    return font;
}
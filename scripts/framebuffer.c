void draw_line(struct multiboot_info* mbd) {
  if (mbd->flags & 1 << 12)
    {
      uint32_t color;
      unsigned i;
      void *fb = (void *) (unsigned long) mbd->framebuffer_addr;

      switch (mbd->framebuffer_type)
        {
        case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
          {
            unsigned best_distance, distance;
            struct multiboot_color *palette;
            
            palette = (struct multiboot_color *) mbd->framebuffer_palette_addr;

            color = 0;
            best_distance = 4*256*256;
            
            for (i = 0; i < mbd->framebuffer_palette_num_colors; i++)
              {
                distance = (0xff - palette[i].blue) * (0xff - palette[i].blue)
                  + palette[i].red * palette[i].red
                  + palette[i].green * palette[i].green;
                if (distance < best_distance)
                  {
                    color = i;
                    best_distance = distance;
                  }
              }
          }
          break;

        case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
          color = ((1 << mbd->framebuffer_blue_mask_size) - 1) 
            << mbd->framebuffer_blue_field_position;
          break;

        case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
          color = '\\' | 0x0100;
          break;

        default:
          color = 0xffffffff;
          break;
        }
      for (i = 0; i < mbd->framebuffer_width
             && i < mbd->framebuffer_height; i++)
        {
          switch (mbd->framebuffer_bpp)
            {
            case 8:
              {
                uint8_t *pixel = fb + mbd->framebuffer_pitch * i + i;
                *pixel = color;
              }
              break;
            case 15:
            case 16:
              {
                uint16_t *pixel
                  = fb + mbd->framebuffer_pitch * i + 2 * i;
                *pixel = color;
              }
              break;
            case 24:
              {
                uint32_t *pixel
                  = fb + mbd->framebuffer_pitch * i + 3 * i;
                *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
              }
              break;

            case 32:
              {
                uint32_t *pixel
                  = fb + mbd->framebuffer_pitch * i + 4 * i;
                *pixel = color;
              }
              break;
            }
        }
    }
}
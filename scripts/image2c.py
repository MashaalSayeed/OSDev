from PIL import Image
import numpy as np

def image_to_header(image_path, header_file_path, array_name):
    # Open the image using PIL
    img = Image.open(image_path)
    
    # Convert the image to RGBA (if it's not already in RGBA mode)
    img = img.convert('RGBA')
    
    # Get image size
    width, height = img.size

    # Convert the image to numpy array for easier processing
    img_data = np.array(img)
    
    # Open the header file for writing
    with open(header_file_path, 'w') as f:
        # Write the array definition and size info to the header
        f.write(f'#ifndef {array_name.upper()}_H\n')
        f.write(f'#define {array_name.upper()}_H\n\n')
        f.write(f'#define {array_name.upper()}_WIDTH {width}\n')
        f.write(f'#define {array_name.upper()}_HEIGHT {height}\n\n')
        f.write(f'unsigned char {array_name}[] = {{\n')
        
        # Write the pixel data to the header file in hexadecimal format
        for row in range(height):
            for col in range(width):
                # Get RGBA values (skip the alpha channel if you don't need it)
                r, g, b, a = img_data[row, col]
                # In this case, we store the RGB as a single 32-bit value
                pixel_value = (r << 16) | (g << 8) | b
                f.write(f'    0x{pixel_value:08X},\n')
        
        f.write(f'}};\n\n')
        f.write(f'#endif // {array_name.upper()}_H\n')

# Usage example:
image_to_header('dirt.png', 'image.h', 'image_data')

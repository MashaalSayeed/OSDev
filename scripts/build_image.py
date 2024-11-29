from PIL import Image
import numpy as np
import argparse

# Function to convert an image to a C header file with pixel data
def image_to_header(image_path, header_file_path, array_name):
    img = Image.open(image_path).convert('RGBA')
    width, height = img.size

    img_data = np.array(img)
    
    with open(header_file_path, 'w') as f:
        # Write the array definition and size info to the header
        f.write(f'#ifndef {array_name.upper()}_H\n')
        f.write(f'#define {array_name.upper()}_H\n\n')
        f.write(f'#define {array_name.upper()}_WIDTH {width}\n')
        f.write(f'#define {array_name.upper()}_HEIGHT {height}\n\n')
        f.write(f'unsigned char {array_name}[] = {{\n')
        
        for row in range(height):
            for col in range(width):
                r, g, b, a = img_data[row, col]
                pixel_value = (r << 16) | (g << 8) | b
                f.write(f'    0x{pixel_value:08X},\n')
        
        f.write(f'}};\n\n')
        f.write(f'#endif // {array_name.upper()}_H\n')


# Usage
# python build_image.py image.png image_data.h image_data

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert an image to a C header file with pixel data.')
    parser.add_argument('image', help='Path to the input image file')
    parser.add_argument('header', help='Path to the output header file')
    parser.add_argument('array_name', default='image_data', help='Name of the array to store pixel data')
    
    args = parser.parse_args()
    image_to_header(args.image, args.header, args.array_name)

    print("Written to:", args.header)

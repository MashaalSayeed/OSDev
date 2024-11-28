#include "io.h"
#include <stddef.h>

#define PORT 0x3f8          // COM1

int init_serial() {
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
   outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
   outb(PORT + 0, 0xAE);    // Send a test byte

   // Check that we received the same test byte we sent
   if(inb(PORT + 0) != 0xAE) {
      return 1;
   }

   // If serial is not faulty set it in normal operation mode:
   // not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled
   outb(PORT + 4, 0x0F);
   return 0;
}

void serial_write(char c) {
    while ((inb(PORT + 5) & 0x20) == 0); // Wait for the transmit buffer to be empty
    outb(PORT, c); // Send the character
}

char serial_read() {
    while ((inb(PORT + 5) & 1) == 0); // Wait for incoming data
    return inb(PORT); // Read the character
}

void log_to_serial(const char* data) {
    for(size_t i = 0; data[i] != '\0'; i++) {
        serial_write(data[i]);
    }
}
#ifndef SCREEN_H
#define SCREEN_H


#define WHITE_ON_BLACK 0x0f

void clear_screen();

int print_char_at(char character, int row, int col);
int print_char(char character);

void print_string_at(char *message, int row, int col);
void print_string(char *message);

#endif
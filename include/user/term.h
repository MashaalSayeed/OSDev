#pragma once

void term_clear_screen();
void term_move_cursor(int row, int col);
void term_set_text_color(int color);
void term_reset_text_color();
void term_get_size(int *rows, int *cols);

int readline(char *buffer, int size);
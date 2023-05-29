void memory_copy(char *source, char *dest, int nbytes) {
    for (int i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

// https://stackoverflow.com/a/3987783
char * int_to_ascii(int num) {
    static char buf[32] = {0};
    int i = 30;

    for(; num && i ; --i, num /= 10)
        buf[i] = "0123456789abcdef"[num % 10];

    return &buf[i+1];
}
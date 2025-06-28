typedef struct window {
    int x; // X coordinate of the window
    int y; // Y coordinate of the window
    int width; // Width of the window
    int height; // Height of the window
    bool isVisible; // Visibility status of the window
    bool focused;
    char title[256]; // Title of the window
} ;
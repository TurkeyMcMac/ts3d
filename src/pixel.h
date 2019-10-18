#ifndef PIXEL_H_
#define PIXEL_H_

// A pixel is an integer from 0-63 of type d3d_pixel.

#include "d3d.h"
#include <curses.h>

// Default transparent pixel when loading textures.
#define EMPTY_PIXEL pixel_from_char(' ')

// Pixel colors for foreground or background.
#define PC_BLACK 0
#define PC_RED 1
#define PC_GREEN 2
#define PC_BLUE 4
#define PC_YELLOW (PC_RED | PC_GREEN)
#define PC_MAGENTA (PC_RED | PC_BLUE)
#define PC_CYAN (PC_GREEN | PC_BLUE)
#define PC_WHITE (PC_RED | PC_GREEN | PC_BLUE)

// Create a pixel with foreground and background colors described above.
#define pixel(fg, bg) (((fg) << 3) | (bg))

// Convert a pixel to a number to be passed to curses COLOR_PAIR.
#define pixel_pair(pix) ((pix) + 1)
// Convert a pixel to a style to be used with mvaddch or something.
#define pixel_style(pix) COLOR_PAIR(pixel_pair(pix))

// The foreground value of the pixel from 0-7.
#define pixel_fg(pix) ((pix) >> 3 & 7)
// The background value of the pixel from 0-7.
#define pixel_bg(pix) ((pix) & 7)

// Convert a character to a pixel.
#define pixel_from_char(ch) ((d3d_pixel)((ch) - ' ' - 1))

#endif /* PIXEL_H_ */

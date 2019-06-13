#ifndef PIXEL_H_
#define PIXEL_H_

// A pixel is an integer from 0-63 of type d3d_pixel.

#include "d3d.h"

// Completely black (for walls) or transparent (for sprites, by default) pixel.
#define EMPTY_PIXEL ((d3d_pixel)-1)

// The foreground value of the pixel from 0-7.
#define pixel_fg(pix) ((pix) >> 3 & 7)
// The background value of the pixel from 0-7.
#define pixel_bg(pix) ((pix) & 7)
// Whether or not this pixel should be bold on the screen.
#define pixel_is_bold(pix) (pixel_bg(pix) != 0)

// Convert a character to a pixel.
#define pixel_from_char(ch) ((d3d_pixel)((ch) - ' ' - 1))

#endif /* PIXEL_H_ */

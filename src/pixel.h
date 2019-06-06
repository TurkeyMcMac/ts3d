#ifndef PIXEL_H_
#define PIXEL_H_

// A pixel is an integer from 0-63 of type d3d_pixel.

#include "d3d.h"

// A completely black pixel.
#define EMPTY_PIXEL ((d3d_pixel)0)

// The foreground value of the pixel from 0-7.
#define pixel_fg(pix) ((unsigned)(pix) >> 3)
// The background value of the pixel from 0-7.
#define pixel_bg(pix) ((pix) & 7)
// Whether or not this pixel should be bold on the screen.
#define pixel_is_bold(pix) (pixel_bg(pix) != 0)

// Convert a character to a pixel.
d3d_pixel pixel_from_char(char ch);

#endif /* PIXEL_H_ */

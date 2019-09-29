#ifndef PIXEL_H_
#define PIXEL_H_

// A pixel is an integer from 0-63 of type d3d_pixel. The top bit is set.

#include "d3d.h"

// Default transparent pixel when loading textures.
#define EMPTY_PIXEL pixel_from_char(' ')

// Return whether the pixel is colored or a letter. The bunch of functions that
// follow only apply to color pixels.
#define pixel_is_letter(pix) (((pix) & 0x80) == 0)

// The pixel index from 0-63.
#define pixel_index(pix) ((pix) & ~0x80)
// The foreground value of the pixel from 0-7.
#define pixel_fg(pix) ((pix) >> 3 & 7)
// The background value of the pixel from 0-7.
#define pixel_bg(pix) ((pix) & 7)
// Whether or not this pixel should be bold on the screen.
#define pixel_is_bold(pix) (pixel_bg(pix) != 0)

// Convert a character to a color pixel.
#define pixel_from_char(ch) ((d3d_pixel)((ch) - ' ' - 1) | 0x80)
// Convert a character to a letter pixel.
#define pixel_letter(ltr) ltr

#endif /* PIXEL_H_ */

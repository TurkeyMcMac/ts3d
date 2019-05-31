#ifndef PIXEL_H_
#define PIXEL_H_

#include "d3d.h"

#define EMPTY_PIXEL ((d3d_pixel)' ')

#define pixel_fg(pix) ((unsigned)(pix) >> 3)
#define pixel_bg(pix) ((pix) & 7)
#define pixel_is_bold(pix) (pixel_bg(pix) != 0)

d3d_pixel pixel_from_char(char ch);

#endif /* PIXEL_H_ */

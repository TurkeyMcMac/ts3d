#include "pixel.h"

d3d_pixel pixel_from_char(char ch)
{
	d3d_pixel pix = ch - ' ' - 1;
	return pix <= 64 ? pix : ch;
}

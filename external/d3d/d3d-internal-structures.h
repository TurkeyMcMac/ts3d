#ifndef D3D_INTERNAL_STRUCTURES_H_
#define D3D_INTERNAL_STRUCTURES_H_

// This header describes the unstable layouts of the pointer-only structures
// used by the library.

#include "d3d.h"

#ifndef D3D_DONT_OPTIMIZE_SAME_SPRITES
#	define OPTIMIZE_SAME_SPRITES 1
#else
#	define OPTIMIZE_SAME_SPRITES 0
#endif

struct d3d_texture_s {
	// Width and height in pixels
	size_t width, height;
	// Row-major pixels of size width * height
	d3d_pixel pixels[];
};

// This is for drawing multiple sprites.
struct d3d_sprite_order {
	// The distance from the camera
	double dist;
	// The corresponding index into the given d3d_sprite_s list
	size_t index;
};

struct d3d_camera_s {
	// The position of the camera on the board
	d3d_vec_s pos;
	// The field of view in the x (sideways) and y (vertical) screen axes.
	// Measured in radians.
	d3d_vec_s fov;
	// The direction of the camera relative to the positive x direction,
	// increasing counterclockwise. In the range [0, 2π).
	double facing;
	// The width and height of the camera screen, in pixels.
	size_t width, height;
	// The value of an empty pixel on the screen, one whose ray hit nothing.
	d3d_pixel empty_pixel;
	// The last buffer used when sorting sprites, or NULL the first time.
	struct d3d_sprite_order *order;
	// The capacity (allocation size) of the field above.
	size_t order_buf_cap;
#if OPTIMIZE_SAME_SPRITES
	// The last sprite list passed to d3d_draw_sprites. If this is the same
	// the next time, some optimizations are enabled.
	const d3d_sprite_s *last_sprites;
#endif
	// For each row of the screen, the tangent of the angle of that row
	// relative to the center of the screen, in radians
	// For example, the 0th item is tan(fov.y / 2)
	double *tans;
	// For each column of the screen, the distance from the camera to the
	// first wall in that direction. This is calculated when drawing columns
	// and is used when drawing sprites.
	double *dists;
	// The pixels of the screen in row-major order.
	d3d_pixel pixels[];
};

struct d3d_board_s {
	// The width and height of the board, in blocks
	size_t width, height;
	// The blocks of the boards. These are pointers to save space with many
	// identical blocks.
	const d3d_block_s *blocks[];
};

#endif /* D3D_INTERNAL_STRUCTURES_H_ */

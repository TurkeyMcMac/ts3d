#ifndef D3D_H_
#define D3D_H_

#include <stddef.h>

/* COMPILE-TIME OPTIONS
 * --------------------
 *  - D3D_PIXEL_TYPE: Pixels can be any integer type you want, but you MUST
 *    compile both your code and this library with the same D3D_PIXEL_TYPE. The
 *    types from stdint.h are not automatically available. Pixels are
 *    unsigned char by default.
 *  - D3D_UNINITIALIZED_ALLOCATOR: Don't automatically initialize d3d_malloc,
 *    d3d_realloc, and d3d_realloc to the standard library equivalents. If this
 *    symbol is defined, the library user must set the variables themselves
 *    before calling other functions. You NEED NOT compile the client code as
 *    well with this option, although you may need to change your client code
 *    depending on whether this option is set.
 *  - D3D_USE_INTERNAL_STRUCTS: Define this to have access to unstable internal
 *    structure layouts of opaque types used by the library. You NEED NOT
 *    compile d3d.c and your code with the same setting of this option.
 *  - D3D_HEADER_INCLUDE: If this is defined, instead of '#include "d3d.h"',
 *    d3d.c will use '#include D3D_HEADER_INCLUDE'. This is ONLY useful when
 *    compiling d3d.c, not the client code. */

#ifndef D3D_PIXEL_TYPE
#	define D3D_PIXEL_TYPE unsigned char
#endif

/* Custom allocator routines. These have the same contract of behaviour as the
 * corresponding functions in the standard library. If the preprocessor symbol
 * D3D_UNINITIALIZED_ALLOCATOR is not defined, these are automatically set to
 * the corresponding standard library functions. */
extern void *(*d3d_malloc)(size_t);
extern void *(*d3d_realloc)(void *, size_t);
extern void (*d3d_free)(void *);

/* A single numeric pixel, for storing whatever data you provide in a
 * texture. */
typedef D3D_PIXEL_TYPE d3d_pixel;

/* A vector in two dimensions. */
typedef struct {
	double x, y;
} d3d_vec_s;

/* A grid of pixels with certain width and height. */
struct d3d_texture_s;
typedef struct d3d_texture_s d3d_texture;

/* A block on a board consisting of 6 face textures. The indices correspond with
 * the values of d3d_direction. Vertical sides are mirrored if the viewer is
 * looking from the inside of a block outward. */
typedef struct {
	const d3d_texture *faces[6];
} d3d_block_s;

/* A two-dimensional entity with an analog position on the grid. A sprite always
 * faces toward the viewer always. Sprites are centered halfway between the top
 * and bottom of the space. */
typedef struct {
	/* The position of the sprite. */
	d3d_vec_s pos;
	/* The x and y stretch factor of the sprite. If one of these is 1.0,
	 * then the sprite is 1 tile in size in that dimension. */
	d3d_vec_s scale;
	/* The texture of the sprite. */
	const d3d_texture *txtr;
	/* A pixel value that is transparent in the texture. If this is -1, none
	 * of the pixels can be transparent. */
	d3d_pixel transparent;
} d3d_sprite_s;

/* An object representing a view into the world and what it sees. */
struct d3d_camera_s;
typedef struct d3d_camera_s d3d_camera;

/* An object representing a location for blocks, sprites, and a camera. */
struct d3d_board_s;
typedef struct d3d_board_s d3d_board;

/* A direction. The possible values are all listed below. */
typedef enum {
	D3D_DNORTH,
	D3D_DSOUTH,
	D3D_DWEST,
	D3D_DEAST,
	D3D_DUP,
	D3D_DDOWN
} d3d_direction;

/* Allocate a new camera with given field of view in the x and y directions (in
 * radians) and a given view width and height in pixels. */
d3d_camera *d3d_new_camera(
	double fovx,
	double fovy,
	size_t width,
	size_t height);

/* Get the view width of a camera in pixels. */
size_t d3d_camera_width(const d3d_camera *cam);

/* Get the view height of a camera in pixels. */
size_t d3d_camera_height(const d3d_camera *cam);

/* Return a pointer to the empty pixel. This is the pixel value set in the
 * camera when a cast ray hits nothing (it gets off the edge of the board.) The
 * pointer is valid for the lifetime of the camera. */
d3d_pixel *d3d_camera_empty_pixel(d3d_camera *cam);

/* Return a pointer to the camera's position. It is UNDEFINED BEHAVIOR for this
 * to be outside the limits of the board that is passed when d3d_draw_walls or
 * d3d_draw_column is called. The pointer is valid for the lifetime of the
 * camera, but the position may be changed when other functions modify the
 * camera. */
d3d_vec_s *d3d_camera_position(d3d_camera *cam);

/* Return a pointer to the camera's direction (in radians). This can be changed
 * without regard for any range. The pointer is valid for the lifetime of the
 * camera, but the angle may be changed when other functions modify the camera.
 */
double *d3d_camera_facing(d3d_camera *cam);

/* Get a pixel in the camera's view, AFTER a drawing function is called on the
 * camera. This returns NULL if the coordinates are out of range. Otherwise, the
 * pointer is valid until another function modifies the camera. */
const d3d_pixel *d3d_camera_get(const d3d_camera *cam, size_t x, size_t y);

/* Destroy a camera object. It shall never be used again. */
void d3d_free_camera(d3d_camera *cam);

/* Allocate a new texture without its pixels initialized. */
d3d_texture *d3d_new_texture(size_t width, size_t height);

/* Get the width of the texture in pixels. */
size_t d3d_texture_width(const d3d_texture *txtr);

/* Get the height of the texture in pixels. */
size_t d3d_texture_height(const d3d_texture *txtr);

/* Return the texture's pixel buffer, the size of its width times its height.
 * The pixels can be initialized in this way. Columns are contiguous, not rows.
 * The pointer is valid until the texture is used in a d3d_block_s or a
 * d3d_sprite_s. */
d3d_pixel *d3d_get_texture_pixels(d3d_texture *txtr);

/* Get a pixel at a coordinate on a texture. NULL is returned if the coordinates
 * are out of range. The pointer is valid until the texture is used in a
 * d3d_block_s or a d3d_sprite_s. */
d3d_pixel *d3d_texture_get(d3d_texture *txtr, size_t x, size_t y);

/* Permanently destroy a texture. */
void d3d_free_texture(d3d_texture *txtr);

/* Create a new board with a width and height. All its blocks are initially
 * empty, transparent on all sides. */
d3d_board *d3d_new_board(size_t width, size_t height);

/* Get the width of the board in blocks. */
size_t d3d_board_width(const d3d_board *board);

/* Get the height of the board in blocks. */
size_t d3d_board_height(const d3d_board *board);

/* Get a block in a board. If the coordinates are out of range, NULL is
 * returned. Otherwise, a pointer to a block POINTER is returned. This pointed-
 * to pointer can be modified with a new block pointer. The outer pointer is
 * valid until the board is used in d3d_draw_column or d3d_draw_walls. */
const d3d_block_s **d3d_board_get(d3d_board *board, size_t x, size_t y);

/* Permanently destroy a board. */
void d3d_free_board(d3d_board *board);

/* FRAMES
 * ------
 * Drawing a frame consists of
 *  1. Calling either d3d_draw_walls or d3d_start_frame then d3d_draw_column for
 *     each column.
 *  2. Calling d3d_draw_sprite/d3d_draw_sprite/d3d_draw_sprite_dist if you need
 *     to draw some sprites. */

/* Begin drawing a frame. This is only necessary if d3d_draw_walls is not
 * called. */
void d3d_start_frame(d3d_camera *cam);

/* Draw a single column of the view, NOT including sprites. All columns must be
 * drawn before examining the pixels of a camera. */
void d3d_draw_column(d3d_camera *cam, const d3d_board *board, size_t x);

/* Draw all the wall columns for the view, making it valid to access pixels. */
void d3d_draw_walls(d3d_camera *cam, const d3d_board *board);

/* Draw a set of sprites in the world. The view is blocked by walls and other
 * closer sprites. */
void d3d_draw_sprites(
	d3d_camera *cam,
	size_t n_sprites,
	const d3d_sprite_s sprites[]);

/* Draw a single sprite. This DOES NOT take into account the overlapping of
 * previously drawn closer sprites; it is just drawn closer than them. You
 * probably want d3d_draw_sprites. */
void d3d_draw_sprite(d3d_camera *cam, const d3d_sprite_s *sp);

/* This is the same as d3d_draw_sprite above, but it uses a distance  already
 * calculated. This must be the distance from the camera to the sprite. */
void d3d_draw_sprite_dist(d3d_camera *cam, const d3d_sprite_s *sp, double dist);

#endif /* D3D_H_ */

/* Complete structure definitions. */
#if defined(D3D_USE_INTERNAL_STRUCTS) && !defined(D3D_INTERNAL_H_)
#define D3D_INTERNAL_H_

struct d3d_texture_s {
	// Width and height in pixels.
	size_t width, height;
	// Column-major pixels.
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
	// The block containing all empty textures.
	d3d_block_s blank_block;
	// The last buffer used when sorting sprites, or NULL the first time.
	struct d3d_sprite_order *order;
	// The capacity (allocation size) of the field above.
	size_t order_buf_cap;
	// The last sprites drawn.
	const d3d_sprite_s *last_sprites;
	// The number of sprites last drawn.
	size_t last_n_sprites;
	// For each row of the screen, the tangent of the angle of that row
	// relative to the center of the screen, in radians
	// For example, the 0th item is tan(fov.y / 2)
	double *tans;
	// For each column of the screen, the distance from the camera to the
	// first wall in that direction. This is calculated when drawing columns
	// and is used when drawing sprites.
	double *dists;
	// The pixels of the screen in column-major order.
	d3d_pixel pixels[];
};

struct d3d_board_s {
	// The width and height of the board, in blocks
	size_t width, height;
	// The blocks of the boards. These are pointers to save space with many
	// identical blocks.
	const d3d_block_s *blocks[];
};

#endif /* defined(D3D_USE_INTERNAL_STRUCTS) && !defined(D3D_INTERNAL_H_) */

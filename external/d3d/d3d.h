#ifndef D3D_H_
#define D3D_H_

#include <stddef.h>
#include <stdint.h>

/* COMPILE-TIME OPTIONS
 * --------------------
 *  - D3D_PIXEL_TYPE: Pixels can be any integer type you want, but you MUST
 *    compile both your code and this library with the same D3D_PIXEL_TYPE. You
 *    can define D3D_PIXEL_TYPE as one of the integer types from stdint.h.
 *    Pixels are uint8_t by default.
 *  - D3D_UNINITIALIZED_ALLOCATOR: Don't automatically initialize d3d_malloc,
 *    d3d_realloc, and d3d_realloc to the standard library equivalents. If this
 *    symbol is defined, the library user must set the variables themselves
 *    before calling other functions. You NEED NOT compile the client code as
 *    well with this option, although you may need to change your client code
 *    depending on whether this option is set.
 *  - D3D_DONT_OPTIMIZE_SAME_SPRITES: Define this to disable optimization when
 *    the same sprites are passed the d3d_draw_sprites twice in a row. In the
 *    default case, when this happens, sprites are assumed not to move much
 *    between frames, which can hurt performance if that is seldom true. You
 *    NEED NOT compile the client code with this option, since the interface is
 *    the same either way. */

#ifndef D3D_PIXEL_TYPE
#	define D3D_PIXEL_TYPE uint8_t
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
 * the values of d3d_direction. Vertical sides are oriented the same to the
 * viewer (i.e. not mirrored) no matter the viewer position. */
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
 * camera when a cast ray hits nothing (it gets off the edge of the board.) */
d3d_pixel *d3d_camera_empty_pixel(d3d_camera *cam);

/* Return a pointer to the camera's position. */
d3d_vec_s *d3d_camera_position(d3d_camera *cam);

/* Return a pointer to the camera's direction (in radians). This can be changed
 * without regard for any range. */
double *d3d_camera_facing(d3d_camera *cam);

/* Get a pixel in the camera's view, AFTER a drawing function is called on the
 * camera. This returns NULL if the coordinates are out of range. */
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
 */
d3d_pixel *d3d_get_texture_pixels(d3d_texture *txtr);

/* Get a pixel at a coordinate on a texture. NULL is returned if the coordinates
 * are out of range. */
d3d_pixel *d3d_texture_get(d3d_texture *txtr, size_t x, size_t y);

/* Create a new board with a width and height. All its blocks are initially
 * empty, transparent on all sides. */
d3d_board *d3d_new_board(size_t width, size_t height);

/* Get the width of the board in blocks. */
size_t d3d_board_width(const d3d_board *board);

/* Get the height of the board in blocks. */
size_t d3d_board_height(const d3d_board *board);

/* Get a block in a board. If the coordinates are out of range, NULL is
 * returned. Otherwise, a pointer to a block POINTER is returned. This pointed-
 * to pointer can be modified with a new block pointer. */
const d3d_block_s **d3d_board_get(d3d_board *board, size_t x, size_t y);

/* FRAMES
 * ------
 * Drawing a frame consists of
 *  - d3d_start_frame
 *  - d3d_draw_walls or d3d_draw_column for each column
 *  - Possibly d3d_draw_sprites or d3d_draw_sprite */

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

#endif /* D3D_H_ */

#include "d3d.h"
#include "d3d-internal-structures.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// get a pointer to a coordinate in a grid. A grid is any structure with members
// 'width' and 'height' and another member 'member' which is a buffer containing
// all items in row-major organization. Parameters may be evaluated multiple
// times. If either coordinate is outside the range, NULL is returned.
#define GET(grid, member, x, y) ((size_t)(x) < (grid)->width && (size_t)(y) < \
		(grid)->height ? \
	&(grid)->member[((size_t)(y) + (grid)->height * (size_t)(x))] : NULL)

#ifndef M_PI
	// M_PI is not always defined it seems
#	define M_PI 3.14159265358979323846
#endif

#ifndef D3D_UNINITIALIZED_ALLOCATOR
void *(*d3d_malloc)(size_t) = malloc;
void *(*d3d_realloc)(void *, size_t) = realloc;
void (*d3d_free)(void *) = free;
#endif

// Computes fmod(n, 1.0) if n >= 0, or 1.0 + fmod(n, 1.0) if n < 0
// Returns in the range [0, 1)
static double mod1(double n)
{
	return n - floor(n);
}

// computes approximately 1.0 - fmod(n, 1.0) if n >= 0 or -fmod(n, 1.0) if n < 0
// Returns in the range [0, 1)
static double revmod1(double n)
{
	return ceil(n) - n;
}

// Compute the difference between the two angles (each themselves between 0 and
// 2π.) The result is between -π and π.
static double angle_diff(double a1, double a2)
{
	double diff = a1 - a2;
	if (diff > M_PI) return -2 * M_PI + diff;
	if (diff < -M_PI) return -diff - 2 * M_PI;
	return diff;
}

// This is pretty much floor(c). However, when c is a nonzero whole number and
// positive is true (that is, the relevant component of the delta position is
// over 0,) c is decremented and returned.
static size_t tocoord(double c, bool positive)
{
	double f = floor(c);
	if (positive && f == c && c != 0.0) return (size_t)c - 1;
	return f;
}

// Move the given coordinates in the direction given. If the direction is not
// cardinal, nothing happens. No bounds are checked.
static void move_dir(d3d_direction dir, size_t *x, size_t *y)
{
	switch(dir) {
	case D3D_DNORTH: --*y; break;
	case D3D_DSOUTH: ++*y; break;
	case D3D_DEAST: ++*x; break;
	case D3D_DWEST: --*x; break;
	default: break;
	}
}

// Rotate a cardinal direction 180°. If the direction is not cardinal, it is
// returned unmodified.
static d3d_direction invert_dir(d3d_direction dir)
{
	switch(dir) {
	case D3D_DNORTH: return D3D_DSOUTH;
	case D3D_DSOUTH: return D3D_DNORTH;
	case D3D_DEAST: return D3D_DWEST;
	case D3D_DWEST: return D3D_DEAST;
	default: return dir;
	}
}

// All boards are initialized by being filled with this block. It is transparent
// on all sides.
static const d3d_block_s empty_block = {{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
}};

d3d_camera *d3d_new_camera(
		double fovx,
		double fovy,
		size_t width,
		size_t height)
{
	size_t base_size, pixels_size, tans_size, dists_size, size;
	d3d_camera *cam;
	base_size = offsetof(d3d_camera, pixels);
	pixels_size = width * height * sizeof(d3d_pixel);
	pixels_size += sizeof(double) - pixels_size % sizeof(double);
	tans_size = height * sizeof(double);
	dists_size = width * sizeof(double);
	size = base_size + pixels_size + tans_size + dists_size;
	cam = d3d_malloc(size);
	if (!cam) return NULL;
	// The members 'tans' and 'dists' are actually pointers to parts of the
	// same allocation:
	cam->tans = (void *)((char *)cam + size - dists_size - tans_size);
	cam->dists = (void *)((char *)cam + size - dists_size);
	cam->facing = 0.0;
	cam->fov.x = fovx;
	cam->fov.y = fovy;
	cam->width = width;
	cam->height = height;
	cam->empty_pixel = 0;
	cam->order = NULL;
	cam->order_buf_cap = 0;
#if OPTIMIZE_SAME_SPRITES
	cam->last_sprites = NULL;
#endif
	memset(cam->pixels, 0, pixels_size);
	for (size_t y = 0; y < height; ++y) {
		double angle = fovy * ((double)y / height - 0.5);
		cam->tans[y] = tan(angle);
	}
	return cam;
}

void d3d_free_camera(d3d_camera *cam)
{
	if (!cam) return;
	d3d_free(cam->order);
	// 'tans' and 'dists' are freed here too:
	d3d_free(cam);
}

d3d_texture *d3d_new_texture(size_t width, size_t height)
{
	size_t pixels_size = width * height * sizeof(d3d_pixel *);
	d3d_texture *txtr =
		d3d_malloc(offsetof(d3d_texture, pixels) + pixels_size);
	if (!txtr) return NULL;
	txtr->width = width;
	txtr->height = height;
	memset(txtr->pixels, 0, pixels_size);
	return txtr;
}

d3d_board *d3d_new_board(size_t width, size_t height)
{
	size_t blocks_size = width * height * sizeof(d3d_block_s *);
	d3d_board *board =
		d3d_malloc(offsetof(d3d_board, blocks) + blocks_size);
	if (!board) return NULL;
	board->width = width;
	board->height = height;
	for (size_t i = 0; i < width * height; ++i) {
		board->blocks[i] = &empty_block;
	}
	return board;
}

// Move the position pos by dpos (delta position) until a wall is hit. dir is
// set to the face of the block which was hit. If no block is hit, NULL is
// returned.
static const d3d_block_s *hit_wall(
	const d3d_board *board,
	d3d_vec_s *pos,
	const d3d_vec_s *dpos,
	d3d_direction *dir)
{
	const d3d_block_s *block;
	do {
		size_t x, y;
		const d3d_block_s * const *blk = NULL;
		d3d_vec_s tonext = {INFINITY, INFINITY};
		d3d_direction ns = D3D_DNORTH, ew = D3D_DWEST;
		if (dpos->x < 0.0) {
			// The ray is going west
			tonext.x = -mod1(pos->x);
		} else if (dpos->x > 0.0) {
			// The ray is going east
			ew = D3D_DEAST;
			tonext.x = revmod1(pos->x);
		}
		if (dpos->y < 0.0) {
			// The ray is going north
			tonext.y = -mod1(pos->y);
		} else if (dpos->y > 0.0) {
			// They ray is going south
			ns = D3D_DSOUTH;
			tonext.y = revmod1(pos->y);
		}
		if (tonext.x / dpos->x < tonext.y / dpos->y) {
			// The ray will hit a east/west wall first
			*dir = ew;
			pos->x += tonext.x;
			pos->y += tonext.x / dpos->x * dpos->y;
		} else {
			// The ray will hit a north/south wall first
			*dir = ns;
			pos->y += tonext.y;
			pos->x += tonext.y / dpos->y * dpos->x;
		}
		x = tocoord(pos->x, dpos->x > 0.0);
		y = tocoord(pos->y, dpos->y > 0.0);
		blk = GET(board, blocks, x, y);
		if (!blk) return NULL; // The ray left the board
		if (!(*blk)->faces[*dir]) {
			// The face the ray hit is empty
			move_dir(*dir, &x, &y);
			blk = GET(board, blocks, x, y);
			if (!blk) return NULL; // The ray left the board
			if (!(*blk)->faces[invert_dir(*dir)]) {
				// The face the ray hit is empty
				// Nudge the ray past the wall:
				if (*dir == ew) {
					pos->x += copysign(0.0001, dpos->x);
				} else {
					pos->y += copysign(0.0001, dpos->y);
				}
				// No wall was hit this time; UP indicates that:
				*dir = D3D_DUP;
			}
		}
		block = *blk;
	} while (*dir == D3D_DUP); // While no wall was hit (UP indicates this.)
	return block;
}

void d3d_draw_column(d3d_camera *cam, const d3d_board *board, size_t x)
{
	d3d_direction face;
	const d3d_block_s *block;
	d3d_vec_s pos = cam->pos, disp;
	double dist;
	double angle =
		cam->facing + cam->fov.x * (0.5 - (double)x / cam->width);
	d3d_vec_s dpos = {cos(angle) * 0.001, sin(angle) * 0.001};
	block = hit_wall(board, &pos, &dpos, &face);
	if (!block) {
		// The ray hit no block, so fill this column with emptiness:
		for (size_t y = 0; y < cam->height; ++y) {
			*GET(cam, pixels, x, y) = cam->empty_pixel;
		}
		return;
	}
	disp.x = pos.x - cam->pos.x;
	disp.y = pos.y - cam->pos.y;
	dist = sqrt(disp.x * disp.x + disp.y * disp.y);
	cam->dists[x] = dist;
	for (size_t t = 0; t < cam->height; ++t) {
		const d3d_texture *txtr;
		size_t tx, ty;
		// The distance the ray travelled, assuming it hit a vertical
		// wall:
		double dist_y = cam->tans[t] * dist + 0.5;
		if (dist_y > 0.0 && dist_y < 1.0) {
			// A vertical wall was indeed hit
			double dimension;
			txtr = block->faces[face];
			// Choose the x coordinate of the pixel depending on
			// wall orientation:
			switch (face) {
			case D3D_DSOUTH:
				dimension = mod1(pos.x);
				break;
			case D3D_DNORTH:
				dimension = revmod1(pos.x);
				break;
			case D3D_DWEST:
				dimension = mod1(pos.y);
				break;
			case D3D_DEAST:
				dimension = revmod1(pos.y);
				break;
			default:
				continue;
			}
			tx = dimension * txtr->width;
			ty = txtr->height * dist_y;
		} else {
			// A floor or ceiling was hit instead. The horizontal
			// displacement is adjusted accordingly:
			double newdist = 0.5 / fabs(cam->tans[t]);
			d3d_vec_s newpos = {
				cam->pos.x + disp.x / dist * newdist,
				cam->pos.y + disp.y / dist * newdist
			};
			size_t bx = tocoord(newpos.x, dpos.x > 0.0),
			       by = tocoord(newpos.y, dpos.y > 0.0);
			const d3d_block_s *top_bot =
				*GET(board, blocks, bx, by);
			if (dist_y >= 1.0) {
				// A ceiling was hit
				txtr = top_bot->faces[D3D_DUP];
				if (!txtr) goto no_texture;
				tx = revmod1(newpos.x) * txtr->width;
			} else {
				// A floor was hit
				txtr = top_bot->faces[D3D_DDOWN];
				if (!txtr) goto no_texture;
				tx = mod1(newpos.x) * txtr->width;
			}
			ty = mod1(newpos.y) * txtr->height;
		}
		*GET(cam, pixels, x, t) = *GET(txtr, pixels, tx, ty);
		continue;

	no_texture:
		*GET(cam, pixels, x, t) = cam->empty_pixel;
	}
}

void d3d_start_frame(d3d_camera *cam)
{
	cam->facing = fmod(cam->facing, 2 * M_PI);
	if (cam->facing < 0.0) {
		cam->facing += 2 * M_PI;
	}
}

void d3d_draw_walls(d3d_camera *cam, const d3d_board *board)
{
	d3d_start_frame(cam);
	for (size_t x = 0; x < cam->width; ++x) {
		d3d_draw_column(cam, board, x);
	}
}

// Draw a sprite with a pre-calculated distance from the camera.
static void draw_sprite(d3d_camera *cam, const d3d_sprite_s *sp, double dist)
{
	d3d_vec_s disp = { sp->pos.x - cam->pos.x, sp->pos.y - cam->pos.y };
	double angle, width, height, diff, maxdiff;
	long start_x, start_y;
	if (dist == 0.0) return;
	// The angle of the sprite relative to the +x axis:
	angle = atan2(disp.y, disp.x);
	// The view width of the sprite in radians:
	width = atan(sp->scale.x / dist) * 2;
	// The max camera-sprite angle difference so the sprite's visible:
	maxdiff = (cam->fov.x + width) / 2;
	diff = angle_diff(cam->facing, angle);
	if (fabs(diff) > maxdiff) return;
	// The height of the sprite in pixels on the camera screen:
	height = atan(sp->scale.y / dist) * 2 / cam->fov.y * cam->height;
	// The width of the sprite in pixels on the camera screen:
	width = width / cam->fov.x * cam->width;
	// The first x where the sprite appears on the screen:
	start_x = (cam->width - width) / 2 + diff / cam->fov.x * cam->width;
	// The first y where the sprite appears on the screen:
	start_y = (cam->height - height) / 2;
	for (size_t x = 0; x < width; ++x) {
		// cx is the x on the camera screen; sx is the x on the sprite's
		// texture:
		size_t cx, sx;
		cx = x + start_x;
		if (cx >= cam->width || dist >= cam->dists[cx]) continue;
		sx = (double)x / width * sp->txtr->width;
		for (size_t y = 0; y < height; ++y) {
			// cy and sy correspond to cx and sx above:
			size_t cy, sy;
			cy = y + start_y;
			if (cy >= cam->height) continue;
			sy = (double)y / height * sp->txtr->height;
			d3d_pixel p = *GET(sp->txtr, pixels, sx, sy);
			if (p != sp->transparent) *GET(cam, pixels, cx, cy) = p;
		}
	}
}

// Compare the sprite orders (see below). This is meant for qsort.
static int compar_sprite_order(const void *a, const void *b)
{
	double dist_a = *(double *)a, dist_b = *(double *)b;
	if (dist_a > dist_b) return 1;
	if (dist_a < dist_b) return -1;
	return 0;
}

void d3d_draw_sprites(
	d3d_camera *cam,
	size_t n_sprites,
	const d3d_sprite_s sprites[])
{
	size_t i;
#if OPTIMIZE_SAME_SPRITES
	if (sprites == cam->last_sprites && n_sprites == cam->order_buf_cap) {
		// This assumes the sprites didn't move much, and are mostly
		// sorted. Therefore, insertion sort is used.
		for (i = 0; i < n_sprites; ++i) {
			size_t move_to;
			struct d3d_sprite_order ord = cam->order[i];
			size_t s = ord.index;
			d3d_vec_s disp = {
				sprites[s].pos.x - cam->pos.x,
				sprites[s].pos.y - cam->pos.y
			};
			ord.dist = hypot(disp.x, disp.y);
			move_to = i;
			for (long j = (long)i - 1; j >= 0; --j) {
				if (cam->order[j].dist > ord.dist) {
					cam->order[j + 1] = cam->order[j];
					move_to = j;
				} else {
					break;
				}
			}
			cam->order[move_to] = ord;
		}
	} else
#endif
	{
		if (n_sprites > cam->order_buf_cap) {
			struct d3d_sprite_order *new_order = d3d_realloc(
				cam->order, n_sprites * sizeof(*cam->order));
			if (new_order) {
				cam->order = new_order;
				cam->order_buf_cap = n_sprites;
			} else {
				// XXX Silently truncate the list of sprites
				// drawn. This may be a bad decision, but
				// failure is unlikely and this shouldn't break
				// any client code.
				n_sprites = cam->order_buf_cap;
			}
		}
		for (i = 0; i < n_sprites; ++i) {
			struct d3d_sprite_order *ord = &cam->order[i];
			d3d_vec_s disp = {
				sprites[i].pos.x - cam->pos.x,
				sprites[i].pos.y - cam->pos.y
			};
			ord->dist = hypot(disp.x, disp.y);
			ord->index = i;
		}
		qsort(cam->order, n_sprites, sizeof(*cam->order),
			compar_sprite_order);
	}
	i = n_sprites;
	while (i--) {
		struct d3d_sprite_order *ord = &cam->order[i];
		draw_sprite(cam, &sprites[ord->index], ord->dist);
	}
}

void d3d_draw_sprite(d3d_camera *cam, const d3d_sprite_s *sp)
{
	draw_sprite(cam, sp,
		hypot(sp->pos.x - cam->pos.x, sp->pos.y - cam->pos.y));
}

size_t d3d_camera_width(const d3d_camera *cam)
{
	return cam->width;
}

size_t d3d_camera_height(const d3d_camera *cam)
{
	return cam->height;
}

d3d_pixel *d3d_camera_empty_pixel(d3d_camera *cam)
{
	return &cam->empty_pixel;
}

d3d_vec_s *d3d_camera_position(d3d_camera *cam)
{
	return &cam->pos;
}

double *d3d_camera_facing(d3d_camera *cam)
{
	return &cam->facing;
}

const d3d_pixel *d3d_camera_get(const d3d_camera *cam, size_t x, size_t y)
{
	return GET(cam, pixels, x, y);
}

size_t d3d_texture_width(const d3d_texture *txtr)
{
	return txtr->width;
}

size_t d3d_texture_height(const d3d_texture *txtr)
{
	return txtr->height;
}

d3d_pixel *d3d_get_texture_pixels(d3d_texture *txtr)
{
	return txtr->pixels;
}

d3d_pixel *d3d_texture_get(d3d_texture *txtr, size_t x, size_t y)
{
	return GET(txtr, pixels, x, y);
}

void d3d_free_texture(d3d_texture *txtr)
{
	free(txtr);
}

size_t d3d_board_width(const d3d_board *board)
{
	return board->width;
}

size_t d3d_board_height(const d3d_board *board)
{
	return board->height;
}

const d3d_block_s **d3d_board_get(d3d_board *board, size_t x, size_t y)
{
	return GET(board, blocks, x, y);
}

void d3d_free_board(d3d_board *board)
{
	free(board);
}

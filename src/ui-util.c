#include "ui-util.h"
#include "pixel.h"
#include "util.h"
#include "xalloc.h"
#include <string.h>

void color_map_init(struct color_map *map)
{
	map->pair_num = 0;
	memset(map->pixel2pair, 0, sizeof(map->pixel2pair));
}

int color_map_add_pair(struct color_map *map, d3d_pixel pix)
{
	if (pix >= sizeof(map->pixel2pair)) return 0;
	if (map->pixel2pair[pix] == 0) {
		if (map->pair_num >= COLOR_PAIRS - 1) return 0;
		++map->pair_num;
		map->pixel2pair[pix] = map->pair_num;
	}
	return map->pixel2pair[pix];
}

int color_map_apply(struct color_map *map)
{
	static const short colors[] = {
		COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
		COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
	};
	int ret = 0;
	for (d3d_pixel p = 0; p < sizeof(map->pixel2pair); ++p) {
		int pair = map->pixel2pair[p];
		if (pair > 0) {
			int fg = colors[pixel_fg(p)];
			int bg = colors[pixel_bg(p)];
			if (init_pair(pair, fg, bg) == ERR) {
				map->pixel2pair[p] = 0;
				ret = -1;
			}
		}
	}
	return ret;
}

size_t color_map_count_pairs(struct color_map *map)
{
	return (size_t)map->pair_num;
}

int color_map_get_pair(struct color_map *map, d3d_pixel pix)
{
	return pix < sizeof(map->pixel2pair) ? map->pixel2pair[pix] : 0;
}

void color_map_destroy(struct color_map *UNUSED_VAR(map))
{
}

void meter_draw(const struct meter *meter)
{
	int full = meter->width * meter->fraction;
	int i;
	for (i = 0; meter->label[i] && i < full; ++i) {
		mvwaddch(meter->win, meter->y, meter->x + i,
			meter->full_style | meter->label[i]);
	}
	if (i < full) {
		do {
			mvwaddch(meter->win,meter->y, meter->x + i,
				meter->full_style | ' ');
		} while (++i < full);
	} else {
		for (; meter->label[i]; ++i) {
			mvwaddch(meter->win, meter->y, meter->x + i,
				meter->empty_style | meter->label[i]);
		}
	}
	for (; i < meter->width; ++i) {
		mvwaddch(meter->win, meter->y, meter->x + i,
			meter->empty_style | ' ');
	}
}

WINDOW *popup_window(const char *text)
{
	int width = 0, height = 0;
	int line_width = 0;
	for (const char *chrp = text; *chrp != '\0'; ++chrp) {
		if (*chrp == '\n') {
			++height;
			line_width = 0;
		} else {
			++line_width;
			if (line_width > width) width = line_width;
		}
	}
	// Simulate a final newline if it's missing:
	if (line_width != 0) ++height;
	// Add one cell of padding to all sides:
	width += 2;
	height += 2;
	// No popups larger than the screen are allowed:
	if (height > LINES || width > COLS) return NULL;
	WINDOW *win = newwin(height, width,
		(LINES - height) / 2, (COLS - width) / 2);
	if (!win) return NULL;
	const char *line = text;
	for (int y = 1; y < height - 1; ++y) {
		const char *nl = strchr(line, '\n');
		// Line length is remaining text length if there's no newline:
		int len = nl ? nl - line : (int)strlen(line);
		// Draw centered line of text:
		mvwaddnstr(win, y, (width - len) / 2, line, len);
		line += len + 1;
	}
	return win;
}

void display_frame(d3d_camera *cam, WINDOW *win, struct color_map *colors)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			int pair = color_map_get_pair(colors, pix);
			mvwaddch(win, y, x, COLOR_PAIR(pair) | ':');
		}
	}
}

// The width:height ratio of each pixel. Curses seems not to have a way to
// determine this dynamically. The information is used when deciding camera
// dimensions.
#define PIXEL_ASPECT 0.625
// The field of view side-to-side, in radians. The field is smaller if the
// screen is taller than it is wide, which is unlikely. See camera_with_dims
// for details.
#define FOV_X 2.0

d3d_camera *camera_with_dims(int width, int height)
{
	d3d_camera *cam;
	if (width <= 0) width = 1;
	if (height <= 0) height = 1;
	if (width >= height) {
		cam = assert_alloc(d3d_new_camera(FOV_X,
			FOV_X / PIXEL_ASPECT * height / width, width, height,
			pixel(PC_BLACK, PC_BLACK)));
	} else {
		cam = assert_alloc(d3d_new_camera(FOV_X * width / height,
			FOV_X / PIXEL_ASPECT, width, height,
			pixel(PC_BLACK, PC_BLACK)));
	}
	return cam;
}

bool sync_screen_size(int known_lines, int known_cols)
{
	// First, sync with the physical screen if need be:
#ifdef PDCURSES
	// PDCurses needs this here to update LINES and COLS:
	if (is_termresized()) resize_term(0, 0);
#endif
	// ncurses and the netbsd curses port I have tested with
	// (github.com/sabotage-linux/netbsd-curses) automatically keep LINES
	// and COLS up to date. Other implementations may not detect terminal
	// resizing, but that isn't a huge deal.
	return known_lines != LINES || known_cols != COLS;
}

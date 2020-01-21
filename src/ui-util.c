#include "ui-util.h"
#include "pixel.h"
#include "util.h"
#include <string.h>

void meter_draw(const struct meter *meter)
{
	int full = meter->width * meter->fraction;
	int i;
	for (i = 0; meter->label[i] && i < full; ++i) {
		mvwaddch(meter->win, meter->y, meter->x + i,
			meter->style | meter->label[i]);
	}
	if (i < full) {
		do {
			mvwaddch(meter->win,meter->y, meter->x + i,
				meter->style | ' ');
		} while (++i < full);
	} else {
		for (; meter->label[i]; ++i) {
			mvwaddch(meter->win, meter->y, meter->x + i,
				(A_REVERSE ^ meter->style) | meter->label[i]);
		}
	}
	for (; i < meter->width; ++i) {
		mvwaddch(meter->win, meter->y, meter->x + i,
			(A_REVERSE ^ meter->style) | ' ');
	}
}

WINDOW *popup_window(const char *text)
{
	int width = 0, height = 0;
	int lwidth = 0;
	const char *chrp;
	for (chrp = text; *chrp != '\0'; ++chrp) {
		if (*chrp == '\n') {
			++height;
			lwidth = 0;
		} else {
			++lwidth;
			if (lwidth > width) width = lwidth;
		}
	}
	if (chrp != text && chrp[-1] != '\n') {
		++height;
		if (lwidth > width) width = lwidth;
	}
	width += 2;
	height += 2;
	WINDOW *win = newwin(height, width,
		(LINES - height) / 2, (COLS - width) / 2);
	const char *line = text;
	for (int y = 1; y < height - 1; ++y) {
		const char *nl = strchr(line, '\n');
		int len = nl ? nl - line : (int)strlen(line);
		mvwaddnstr(win, y, (width - len) / 2, line, len);
		line += len + 1;
	}
	return win;
}

void display_frame(d3d_camera *cam, WINDOW *win)
{
	for (size_t x = 0; x < d3d_camera_width(cam); ++x) {
		for (size_t y = 0; y < d3d_camera_height(cam); ++y) {
			d3d_pixel pix = *d3d_camera_get(cam, x, y);
			mvwaddch(win, y, x, pixel_style(pix) | '#');
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
		cam = d3d_new_camera(FOV_X,
			FOV_X / PIXEL_ASPECT * height / width, width, height);
	} else {
		cam = d3d_new_camera(FOV_X * width / height,
			FOV_X / PIXEL_ASPECT, width, height);
	}
	*d3d_camera_empty_pixel(cam) = pixel(PC_BLACK, PC_BLACK);
	return cam;
}

int set_up_colors(void)
{
	static const short colors[] = {
		COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE,
		COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
	};
	if (start_color() == ERR) return -1;
	for (size_t fg = 0; fg < ARRSIZE(colors); ++fg) {
		for (size_t bg = 0; bg < ARRSIZE(colors); ++bg) {
			if (init_pair(pixel_pair(pixel(fg, bg)),
				colors[fg], colors[bg]) == ERR) return -1;
		}
	}
	return 0;
}

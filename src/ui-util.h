#ifndef UI_UTIL_H_
#define UI_UTIL_H_

#include "d3d.h"
#include <curses.h>

// The ASCII escape key.
#define ESC '\033'

// The ASCII delete key.
#define DEL '\177'

// A map from pixel values to Curses color pairs. The fields are private. If all
// possible pixels were initialized, Curses might run out of color pairs.
struct color_map {
	// The number of initialized pairs.
	int pair_num;
	// The mapping from color pixels to color pairs, all items initialized.
	char pixel2pair[64];
};

// Initialize an empty color map.
void color_map_init(struct color_map *map);

// Initialize a pair for the pixel. The return value is the color pair number.
// Pair 0 will be given if the pixel could not be mapped to a color pair.
int color_map_add_pair(struct color_map *map, d3d_pixel pix);

// Register all color pairs with Curses.
int color_map_apply(struct color_map *map);

// Count the number of successfully initialized pairs.
size_t color_map_count_pairs(struct color_map *map);

// Get the color pair number corresponding to a pixel.
int color_map_get_pair(struct color_map *map, d3d_pixel pix);

// Free resources associated with the map.
void color_map_destroy(struct color_map *map);

// Configuration for drawing an analog meter on the screen.
struct meter {
	// The text to label the meter with.
	const char *label;
	// Meter fullness out of 1.
	double fraction;
	// The curses style of the meter.
	int style;
	// The x and y position of the meter, in character cells.
	int x, y;
	// The width of the meter, in character cells.
	int width;
	// The window to draw to.
	WINDOW *win;
};

// Draw the meter.
void meter_draw(const struct meter *meter);

// Create a small window in the middle of the screen. The window will contain
// the given text with each line centered.
WINDOW *popup_window(const char *text);

// Copy the current scene from the camera to the window. The color map is used
// to translate pixels for Curses.
void display_frame(d3d_camera *cam, WINDOW *win, struct color_map *colors);

// Create a camera with the given positive dimensions.
d3d_camera *camera_with_dims(int width, int height);

// Synchronizes Curses with the actual size of the screen. Returns whether the
// screen size was updated.
bool sync_screen_size(int known_lines, int known_cols);

#endif /* UI_UTIL_H_ */

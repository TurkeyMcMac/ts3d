#ifndef UI_UTIL_H_
#define UI_UTIL_H_

#include <curses.h>

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

#endif /* UI_UTIL_H_ */

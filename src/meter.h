#ifndef METER_H_
#define METER_H_

// Configuration for drawing an analog meter on the screen.
struct meter {
	// The text to label the meter with.
	const char *label;
	// The curses style of the meter.
	int style;
	// The x and y position of the meter, in character cells.
	int x, y;
	// The width of the meter, in character cells.
	int width;
};

// Draw the meter a fraction full.
void meter_draw(const struct meter *meter, double fraction);

#endif /* METER_H_ */

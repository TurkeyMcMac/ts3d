#include "meter.h"
#include <curses.h>

void meter_draw(const struct meter *meter, double fraction)
{
	int full = meter->width * fraction;
	int i;
	for (i = 0; meter->label[i] && i < full; ++i) {
		mvaddch(meter->y, meter->x + i, meter->style | meter->label[i]);
	}
	if (i < full) {
		do {
			mvaddch(meter->y, meter->x + i, meter->style | ' ');
		} while (++i < full);
	} else {
		for (; meter->label[i]; ++i) {
			mvaddch(meter->y, meter->x + i,
				(A_REVERSE ^ meter->style) | meter->label[i]);
		}
	}
	for (; i < meter->width; ++i) {
		mvaddch(meter->y, meter->x + i,
			(A_REVERSE ^ meter->style) | ' ');
	}
}

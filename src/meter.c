#include "meter.h"

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

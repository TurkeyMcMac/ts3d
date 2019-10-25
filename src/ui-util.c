#include "ui-util.h"
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

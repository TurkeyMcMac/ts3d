#ifndef LOGGER_H_
#define LOGGER_H_

#include "util.h"
#include <stdbool.h>
#include <stdio.h>

struct logger {
	int flags;
	// Log destinations.
	FILE *info, *warning, *error;
	// Mask of INFO, WARNING, and ERROR telling which to fclose on free.
	int do_free;
};

// Flags for use with logger_printf.
#define LOGGER_INFO		0x0001
#define LOGGER_WARNING		0x0002
#define LOGGER_ERROR		0x0004
#define LOGGER_ALL		(LOGGER_INFO | LOGGER_WARNING | LOGGER_ERROR)
#define LOGGER_NO_PREFIX	0x0008 // To remove "[INFO] " etc.
// Flags for use with logger_set_color
#define LOGGER_COLOR		0x0010
#define LOGGER_NO_COLOR		0x0020
#define LOGGER_AUTO_COLOR	0 // Not actually a flag

// Initialize a logger with default settings: everything going to standard error
// and AUTO_COLOR (colorization based on whether the output is a terminal.)
void logger_init(struct logger *log);

// Get the current output for the log type `which` (INFO, WARNING, or ERROR). If
// the log goes nowhere (output is discarded), NULL is returned.
FILE *logger_get_output(struct logger *log, int which);

// Set an output for `which`, which is INFO, WARNING, or ERROR. If `dest` is
// NULL, messages are never printed. If not and do_free is true, the output will
// be closed when logger_free is called. The previous output is not closed.
void logger_set_output(struct logger *log, int which, FILE *dest, bool do_free);

// Set colorization, which for now affects all three log types. `color` is COLOR
// for color always, NO_COLOR for color never, or AUTO_COLOR for color when the
// output is a terminal.
void logger_set_color(struct logger *log, int color);

// Print a formatted message. `flags` contains one of the log types along with
// perhaps NO_PREFIX, indicating that "[INFO]", "[WARNING]", etc. should not be
// printed at the start of the message.
void logger_printf(struct logger *log, int flags, const char *format, ...)
	ATTRIBUTE(format(printf, 3, 4));

// Free a logger and close its outputs which have been marked as such.
void logger_free(struct logger *log);

#endif /* LOGGER_H_ */

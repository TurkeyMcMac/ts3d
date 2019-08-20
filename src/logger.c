#include "logger.h"
#include <stdarg.h>
#include <unistd.h>

// Private flags for logger.flags.
#define LOGGER_COLOR		0x0010
#define LOGGER_NO_COLOR		0x0020

static int get_mode(int flags)
{
	if (flags & LOGGER_INFO) {
		return LOGGER_INFO;
	} else if (flags & LOGGER_WARNING) {
		return LOGGER_WARNING;
	} else if (flags & LOGGER_ERROR) {
		return LOGGER_ERROR;
	}
	return LOGGER_INFO;
}

static FILE **get_filep(struct logger *log, int mode)
{
	switch (mode) {
	default:
	case LOGGER_INFO:
		return &log->info;
	case LOGGER_WARNING:
		return &log->warning;
	case LOGGER_ERROR:
		return &log->error;
	}
}

static const char *get_color(struct logger *log, int mode)
{
	(void)log;
	switch (mode) {
	default:
	case LOGGER_INFO:
		return "";
	case LOGGER_WARNING:
		return "\x1B[1;35m";
	case LOGGER_ERROR:
		return "\x1B[1;31m";
	}
}

static const char *get_prefix(struct logger *log, int mode)
{
	(void)log;
	switch (mode) {
	default:
	case LOGGER_INFO:
		return "[INFO] ";
	case LOGGER_WARNING:
		return "[WARNING] ";
	case LOGGER_ERROR:
		return "[ERROR] ";
	}
}

void logger_init(struct logger *log)
{
	log->flags = LOGGER_ALL;
	log->info = log->warning = log->error = stderr;
	log->do_free = 0;
}

FILE *logger_get_output(struct logger *log, int which)
{
	return *get_filep(log, get_mode(which));
}

void logger_set_output(struct logger *log, int which, FILE *dest, bool do_free)
{
	int mode = get_mode(which);
	log->do_free &= ~mode;
	*get_filep(log, mode) = dest;
	if (dest) {
		log->flags |= mode;
		if (do_free) log->do_free |= mode;
	} else {
		log->flags &= ~mode;
	}
}

void logger_set_color(struct logger *log, int color)
{
	log->flags &= ~(LOGGER_NO_COLOR | LOGGER_COLOR);
	log->flags |= color & (LOGGER_NO_COLOR | LOGGER_COLOR);
}

void logger_printf(struct logger *log, int flags, const char *format, ...)
{
	int mode = get_mode(flags);
	if (!(log->flags & mode)) return;
	FILE *file = *get_filep(log, mode);
	bool colored = !(log->flags & LOGGER_NO_COLOR)
	            && ((log->flags & LOGGER_COLOR) || isatty(fileno(file)));
	if (colored) {
		fprintf(file, "%s", get_color(log, mode));
	}
	if (!(flags & LOGGER_NO_PREFIX)) {
		fprintf(file, "%s", get_prefix(log, mode));
	}
	va_list args;
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	if (colored) {
		fprintf(file, "\x1B[0m");
	}
}

void logger_free(struct logger *log)
{
	FILE *file;
	if ((log->do_free & LOGGER_INFO)
	 && (file = *get_filep(log, LOGGER_INFO)))
		fclose(file);
	if ((log->do_free & LOGGER_WARNING)
	 && (file = *get_filep(log, LOGGER_WARNING)))
		fclose(file);
	if ((log->do_free & LOGGER_ERROR)
	 && (file = *get_filep(log, LOGGER_ERROR)))
		fclose(file);
}

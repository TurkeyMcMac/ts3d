#include "do-ts3d-game.h"
#include "logger.h"
#include "util.h"
#include "xalloc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static void print_usage(const char *progname, FILE *to)
{
	fprintf(to, "Usage: %s [options] [play_as]\n", progname);
}

static void print_help(const char *progname)
{
	print_usage(progname, stdout);
	puts(
"\n"
"Play the game Thing Shooter 3D.\n"
"\n"
"Options:\n"
"  -d data_dir   Read game data from data_dir.\n"
"  -h            Print this help information.\n"
"  -l level=dest Log messages of the given level (one of all, info, warning,\n"
"                or error) to the destination file. If the file name's empty,\n"
"                messages are printed to stderr.\n"
"  -L level      Do not log messages of the given log level.\n"
"  -s state_file Read persistent state from state_file.\n"
"  -v            Print version information.\n"
"\n"
"The optional argument play_as gives the name of the save to start off with.\n"
"If it does not exist, it will be created. If it is not given, you start out\n"
"anonymous; log in to save your progress!\n"
"Game data and state is looked for in $TS3D_ROOT, or $HOME/.ts3d by default.\n"
"If the root directory doesn't exist, it will be created. The paths for data\n"
"and state specifically can be overriden by $TS3D_DATA/-d and $TS3D_STATE/-s,\n"
"respectively. The files will be created if they do not exist. The options\n"
"can override the environment variables if given.\n"
"ts3d by default logs to the file 'log' in the game's root directory, or to\n"
"the $TS3D_LOG variable if it is set. Again, options override this.\n");
}

static void print_version(const char *progname)
{
	printf("%s version 1.3.2\n", progname);
}

// Marker for log levels indicating that the user did not configure them; they
// will never be configured to write to stdin, so it can be a sentinel value.
#define UNTOUCHED_MARKER stdin

static int add_log_dest(const char *progname, struct logger *log,
	const char *arg)
{
	FILE *dest;
	bool do_free = false;
	const char *eq = strchr(arg, '=');
	if (eq) {
		const char *fname = eq + 1;
		if (*fname) {
			// Open the file to append so FILEs to the same physical
			// file work out as expected (hopefully.)
			if (!(dest = fopen(fname, "w")) // Clear the file.
			 || !(dest = freopen(fname, "a", dest))) {
				fprintf(stderr,
					"%s: Error opening log file %s: %s\n",
					progname, fname, strerror(errno));
				return -1;
			}
			do_free = true;
		} else {
			dest = stderr;
		}
	} else {
		fprintf(stderr,
			"%s: No log level destination given; use -l%s=...\n",
			progname, arg);
		return -1;
	}
	int which;
	size_t name_size = eq - arg;
	if (!strncasecmp("all=", arg, name_size + 1)) {
		which = LOGGER_ALL;
	} else if (!strncasecmp("info=", arg, name_size + 1)) {
		which = LOGGER_INFO;
	} else if (!strncasecmp("warning=", arg, name_size + 1)) {
		which = LOGGER_WARNING;
	} else if (!strncasecmp("error=", arg, name_size + 1)) {
		which = LOGGER_ERROR;
	} else {
		fprintf(stderr, "%s: Invalid log name: %.*s\n",
			progname, (int)name_size, arg);
		return -1;
	}
	logger_set_output(log, which, dest, do_free);
	return 0;
}

static int remove_log_dest(const char *progname, struct logger *log,
	const char *arg)
{
	int which;
	if (!strcmp("all", arg)) {
		remove_log_dest(progname, log, "info");
		remove_log_dest(progname, log, "warning");
		remove_log_dest(progname, log, "error");
		return 0;
	} else if (!strcmp("info", arg)) {
		which = LOGGER_INFO;
	} else if (!strcmp("warning", arg)) {
		which = LOGGER_WARNING;
	} else if (!strcmp("error", arg)) {
		which = LOGGER_ERROR;
	} else {
		fprintf(stderr, "%s: Invalid log name: %s\n", progname, arg);
		return -1;
	}
	FILE *old = logger_get_output(log, which);
	if (old && old != stderr && old != UNTOUCHED_MARKER) fclose(old);
	logger_set_output(log, which, NULL, false);
	return 0;
}

static char *default_file(const char *name, const char *env)
{
	char *dot_dir = NULL, *path = NULL;
	char *env_root, *env_path = getenv(env);
	if (env_path) return str_dup(env_path);
	env_root = getenv("TS3D_ROOT");
	if (env_root) {
		dot_dir = str_dup(env_root);
	} else {
		char *home = getenv("HOME");
		if (home) {
			dot_dir = mid_cat(home, '/', ".ts3d");
		} else {
			errno = ENOENT;
			return NULL;
		}
	}
	if (!ensure_file(dot_dir, true))
		path = mid_cat(dot_dir, '/', name);
	free(dot_dir);
	return path;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	bool error = false; // Indicates option parsing error.
	const char *progname = argc > 0 ? argv[0] : "ts3d";
	char *play_as = NULL, *data_dir = NULL, *state_file = NULL;
	struct logger log;
	char *log_name_def = NULL;
	FILE *log_def = NULL;
	int opt;
	logger_init(&log);
	logger_set_output(&log, LOGGER_ALL, UNTOUCHED_MARKER, false);
	while ((opt = getopt(argc, argv, "d:hl:L:s:v")) >= 0) {
		switch (opt) {
		case 'd':
			free(data_dir);
			data_dir = str_dup(optarg);
			break;
		case 'h':
			print_help(progname);
			ret = 0;
			goto end;
		case 'l':
			if (add_log_dest(progname, &log, optarg)) goto end;
			break;
		case 'L':
			if (remove_log_dest(progname, &log, optarg)) goto end;
			break;
		case 's':
			free(state_file);
			state_file = str_dup(optarg);
			break;
		case 'v':
			print_version(progname);
			ret = 0;
			goto end;
		default:
			error = true;
			break;
		}
	}
	if (error) {
		print_usage(progname, stderr);
		goto end;
	}
	if (optind < argc) play_as = argv[optind];
	if (!data_dir) data_dir = default_file("data", "TS3D_DATA");
	if (!data_dir || ensure_file(data_dir, true)) {
		fprintf(stderr,
			"%s: No suitable \"data\" directory available: %s\n",
			progname, strerror(errno));
		goto end;
	}
	if (!state_file) state_file = default_file("state.json", "TS3D_STATE");
	if (!state_file || ensure_file(state_file, false)) {
		fprintf(stderr,
			"%s: No suitable \"state.json\" file available: %s\n",
			progname, strerror(errno));
		goto end;
	}
	if ((log_name_def = default_file("log", "TS3D_LOG"))
	 && (log_def = fopen(log_name_def, "w"))) {
		if (logger_get_output(&log, LOGGER_INFO) == UNTOUCHED_MARKER)
			logger_set_output(&log, LOGGER_INFO, log_def, false);
		if (logger_get_output(&log, LOGGER_WARNING) == UNTOUCHED_MARKER)
			logger_set_output(&log, LOGGER_WARNING, log_def, false);
		if (logger_get_output(&log, LOGGER_ERROR) == UNTOUCHED_MARKER)
			logger_set_output(&log, LOGGER_ERROR, log_def, false);
	}
	ret = do_ts3d_game(play_as, data_dir, state_file, &log);
end:
	free(data_dir);
	free(state_file);
	logger_free(&log);
	free(log_name_def);
	if (log_def) fclose(log_def);
	exit(ret >= 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

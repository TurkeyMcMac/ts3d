#include "do-ts3d-game.h"
#include "util.h"
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
"can override the environment variables if given.");
}

static void print_version(const char *progname)
{
	printf("%s version 1.2.0\n", progname);
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
	if (!ensure_file(dot_dir, O_RDONLY | O_DIRECTORY))
		path = mid_cat(dot_dir, '/', name);
	free(dot_dir);
	return path;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	bool error = false; // Indicates option parsing error.
	const char *progname = argc > 0 ? argv[0] : "ts3d";
	char *play_as = NULL, *data_dir = NULL, *state_file = NULL;
	int opt;
	while ((opt = getopt(argc, argv, "d:hs:v")) >= 0) {
		switch (opt) {
		case 'd':
			free(data_dir);
			data_dir = str_dup(optarg);
			break;
		case 'h':
			print_help(progname);
			exit(EXIT_SUCCESS);
		case 's':
			free(state_file);
			state_file = str_dup(optarg);
			break;
		case 'v':
			print_version(progname);
			exit(EXIT_SUCCESS);
		default:
			error = true;
			break;
		}
	}
	if (error) {
		print_usage(progname, stderr);
		exit(EXIT_FAILURE);
	}
	if (optind < argc) play_as = argv[optind];
	if (!data_dir) data_dir = default_file("data", "TS3D_DATA");
	if (!data_dir || ensure_file(data_dir, O_RDONLY | O_DIRECTORY)) {
		fprintf(stderr,
			"%s: No suitable \"data\" directory available: %s\n",
			progname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!state_file) state_file = default_file("state.json", "TS3D_STATE");
	if (!state_file || ensure_file(state_file, O_RDONLY)) {
		fprintf(stderr,
			"%s: No suitable \"state.json\" file available: %s\n",
			progname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	ret = do_ts3d_game(play_as, data_dir, state_file);
	free(data_dir);
	free(state_file);
	exit(ret >= 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

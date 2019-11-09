#include "do-ts3d-game.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
"anonymous; log in to save your progress!");
}

static void print_version(const char *progname)
{
	printf("%s version 1.0.0\n", progname);
}

int main(int argc, char *argv[])
{
	bool error = false; // Indicates option parsing error.
	const char *progname = argc > 0 ? argv[0] : "ts3d";
	char *play_as = NULL, *data_dir = "data", *state_file = "state.json";
	int opt;
	while ((opt = getopt(argc, argv, "d:hs:v")) >= 0) {
		switch (opt) {
		case 'd':
			data_dir = optarg;
			break;
		case 'h':
			print_help(progname);
			exit(EXIT_SUCCESS);
		case 's':
			state_file = optarg;
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
	return do_ts3d_game(play_as, data_dir, state_file) ?
		EXIT_FAILURE : EXIT_SUCCESS;
}

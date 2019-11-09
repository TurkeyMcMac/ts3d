#include "do-ts3d-game.h"
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char *play_as = NULL, *data_dir = "data", *state_file = "state.json";
	int opt;
	while ((opt = getopt(argc, argv, "d:s:")) >= 0) {
		switch (opt) {
		case 'd':
			data_dir = optarg;
			break;
		case 's':
			state_file = optarg;
			break;
		}
	}
	if (optind < argc) play_as = argv[optind];
	return do_ts3d_game(play_as, data_dir, state_file) ?
		EXIT_FAILURE : EXIT_SUCCESS;
}

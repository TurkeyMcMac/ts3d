#include "do-ts3d-game.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
	const char *play_as = argc > 1 ? argv[1] : NULL;
	const char *data_dir = "data";
	const char *state_file = "state.json";
	return do_ts3d_game(play_as, data_dir, state_file) ?
		EXIT_FAILURE : EXIT_SUCCESS;
}

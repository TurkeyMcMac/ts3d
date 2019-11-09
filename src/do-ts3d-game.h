#ifndef DO_TS3D_GAME_H_
#define DO_TS3D_GAME_H_

// Run a game of Thing Shooter 3D. This will take control of the terminal.
// play_as is the name of the one to log in as, or NULL for an anonymous
// beginning. data_dir is the path of the game data root directory. state_file
// is the path of the file where persistent state is kept. The return value is 0
// for success or -1 for some kind of failure.
int do_ts3d_game(const char *play_as, const char *data_dir,
	const char *state_file);

#endif /* DO_TS3D_GAME_H_ */

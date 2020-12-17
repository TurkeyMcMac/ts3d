#ifndef SAVE_STATE_H_
#define SAVE_STATE_H_

#include "table.h"
#include <stdio.h>
#include <stdbool.h>

struct logger; // Weak dependency

// A representation of a named level progress record created by the user.
struct save_state;

// A collection of save states with unique names.
struct save_states {
	table saves;
};

// Create a collection from a file. The file is closed. Error messages are sent
// to the log if an error occurs, and -1 is returned (0 is for success.)
int save_states_init(struct save_states *saves, FILE *from, struct logger *log);

// Add a named save state. The save is returned, or NULL is returned if that
// name is used already.
struct save_state *save_states_add(struct save_states *saves, const char *name);

// This is the same as TABLE_FOR_EACH except the table is the one in savesp.
#define SAVE_STATES_FOR_EACH(savesp, name, save) \
	TABLE_FOR_EACH(&(savesp)->saves, name, save)

// Get an unmodifiable name of a save state.
const char *save_state_name(const struct save_state *save);

// Check whether a named level is recorded as complete in the save.
bool save_state_is_complete(const struct save_state *save, const char *name);

// Mark a level name as complete.
void save_state_mark_complete(struct save_state *save, const char *name);

// Get a save state with a given name from the collection. NULL means the state
// does not exist.
struct save_state *save_states_get(struct save_states *saves, const char *name);

// Remove a save from the collection. -1 is returned if the save was not present
// in the first place.
int save_states_remove(struct save_states *saves, const char *name);

// Write save states to a file. The file is NOT closed. -1 is returned if an
// error occurred, and the states are incompletely written.
int save_states_write(struct save_states *saves, FILE *to);

// Deallocate the members of saves.
void save_states_destroy(struct save_states *saves);

#endif /* SAVE_STATE_H_ */

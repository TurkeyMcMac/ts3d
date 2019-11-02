#ifndef SAVE_STATE_H_
#define SAVE_STATE_H_

#include "table.h"
#include "logger.h"
#include <stdio.h>
#include <stdbool.h>

struct save_state;

struct save_states {
	table saves;
};

int save_states_init(struct save_states *saves, FILE *from, struct logger *log);

struct save_state *save_states_add(struct save_states *saves, const char *name);

#define SAVE_STATES_FOR_EACH(saves, name, save) \
	TABLE_FOR_EACH(&(saves)->saves, name, save)

const char *save_state_name(const struct save_state *save);

bool save_state_is_complete(const struct save_state *save, const char *name);

void save_state_mark_complete(struct save_state *save, const char *name);

int save_states_remove(struct save_states *saves, const char *name);

void save_states_destroy(struct save_states *saves);

#endif /* SAVE_STATE_H_ */

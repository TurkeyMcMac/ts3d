#include "save-state.h"
#include "json-util.h"
#include "util.h"
#include "xalloc.h"
#include <stdlib.h>

struct save_state {
	char *name;
	table complete;
};

void save_states_empty(struct save_states *saves)
{
	table_init(&saves->saves, 5);
}

static void parse_save_state(struct save_state *save, struct json_node *json)
{
	union json_node_data *got;
	if (!(got = json_map_get(json, "complete", JN_LIST))) return;
	for (size_t i = 0; i < got->list.n_vals; ++i) {
		if (got->list.vals[i].kind == JN_STRING)
			save_state_mark_complete(save, got->list.vals[i].d.str);
	}
}

static void free_save_state(struct save_state *save)
{
	free(save->name);
	const char *key;
	void **UNUSED_VAR(val);
	TABLE_FOR_EACH(&save->complete, key, val) {
		free((char *)key);
	}
	table_free(&save->complete);
	free(save);
}

int save_states_init(struct save_states *saves, FILE *from, struct logger *log)
{
	struct json_node jtree;
	if (parse_json_tree("save_states", from, log, &jtree)) return -1;
	save_states_empty(saves);
	if (jtree.kind != JN_MAP) goto end;
	union json_node_data *got;
	if (!(got = json_map_get(&jtree, "saves", JN_MAP))) goto end;
	const char *key;
	void **val;
	TABLE_FOR_EACH(&got->map, key, val) {
		parse_save_state(save_states_add(saves, key), *val);
	}
end:
	free_json_tree(&jtree);
	return 0;
}

struct save_state *save_states_add(struct save_states *saves, const char *name)
{
	struct save_state *save = xmalloc(sizeof(*save));
	save->name = str_dup(name);
	table_init(&save->complete, 8);
	if (table_add(&saves->saves, save->name, save)) {
		free_save_state(save);
		return NULL;
	}
	return save;
}

struct save_state *save_states_get(struct save_states *saves, const char *name)
{
	void **got = table_get(&saves->saves, name);
	return got ? *got : NULL;
}

const char *save_state_name(const struct save_state *save)
{
	return save->name;
}

bool save_state_is_complete(const struct save_state *save, const char *name)
{
	return table_get((table *)&save->complete, name) != NULL;
}

void save_state_mark_complete(struct save_state *save, const char *name)
{
	char *name_dup = str_dup(name);
	if (table_add(&save->complete, name_dup, NULL)) free(name_dup);
}

int save_states_remove(struct save_states *saves, const char *name)
{
	struct save_state *save = table_remove(&saves->saves, name);
	if (save) {
		free_save_state(save);
		return 0;
	}
	return -1;
}

static int save_state_write(const struct save_state *save, FILE *to)
{
	TRY(fputs("{\n   \"complete\": ", to));
	if (table_count(&save->complete) > 0) {
		const char *before = "[";
		const char *key;
		void **UNUSED_VAR(val);
		TABLE_FOR_EACH(&save->complete, key, val) {
			TRY(fprintf(to, "%s\n    \"", before));
			TRY(escape_text_json(key, to));
			before = "\",";
		}
		TRY(fputs("\"\n   ]\n  }", to));
	} else {
		TRY(fputs("[]\n  }", to));
	}
	return 0;
}

int save_states_write(struct save_states *saves, FILE *to)
{
	TRY(fputs("{\n \"saves\": ", to));
	if (table_count(&saves->saves) > 0) {
		char before = '{';
		const char *name;
		void **val;
		TABLE_FOR_EACH(&saves->saves, name, val) {
			TRY(fprintf(to, "%c\n  \"", before));
			TRY(escape_text_json(name, to));
			TRY(fputs("\": ", to));
			TRY(save_state_write(*val, to));
			before = ',';
		}
		TRY(fputs("\n }\n}\n", to));
	} else {
		TRY(fputs("{}\n}\n", to));
	}
	return 0;
}

void save_states_destroy(struct save_states *saves)
{
	const char *UNUSED_VAR(key);
	void **val;
	TABLE_FOR_EACH(&saves->saves, key, val) {
		free_save_state(*val);
	}
	table_free(&saves->saves);
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include "logger.h"
#	include "test-file.h"
#	include <assert.h>
#	include <string.h>

static void setup_saves(struct save_states *saves)
{
	char from_text[] =
	"{\"saves\":{"
		"\"PLAYER_1\":{\"complete\":[]},"
		"\"PLAYER_2\":{\"complete\":[\"a\",\"b\",1]}"
	"}}";
	FILE *from = test_input(from_text, sizeof(from_text));
	struct logger logger;
	logger_init(&logger);
	assert(!save_states_init(saves, from, &logger));
}

char *saves_to_string(struct save_states *saves, size_t *n_writ)
{
	char *writ;
	int out_fd;
	FILE *to = test_output(&out_fd);
	save_states_write(saves, to);
	fclose(to);
	test_read_output(out_fd, &writ, n_writ);
	writ = xrealloc(writ, *n_writ + 1);
	writ[*n_writ] = '\0';
	return writ;
}

CTF_TEST(write_save_valid,
	struct save_states saves;
	setup_saves(&saves);
	size_t n_writ;
	char *writ = saves_to_string(&saves, &n_writ);
	struct logger logger;
	logger_init(&logger);
	struct json_node root;
	FILE *source = test_input(writ, n_writ);
	assert(parse_json_tree("(memory)", source, &logger, &root) == 0);
	logger_free(&logger);
	free_json_tree(&root);
	assert(strstr(writ, "PLAYER_1"));
	assert(strstr(writ, "PLAYER_2"));
	save_states_destroy(&saves);
)

CTF_TEST(save_states_adds,
	struct save_states saves;
	setup_saves(&saves);
	assert(!save_states_add(&saves, "PLAYER_1"));
	assert(save_states_add(&saves, "PLAYER_3"));
	size_t n_writ;
	char *writ = saves_to_string(&saves, &n_writ);
	assert(strstr(writ, "PLAYER_1"));
	assert(strstr(writ, "PLAYER_2"));
	assert(strstr(writ, "PLAYER_3"));
	save_states_destroy(&saves);
)

CTF_TEST(save_states_gets,
	struct save_states saves;
	setup_saves(&saves);
	assert(!save_states_get(&saves, "PLAYER_3"));
	assert(!strcmp("PLAYER_1",
		save_state_name(save_states_get(&saves, "PLAYER_1"))));
	save_states_destroy(&saves);
)

CTF_TEST(save_states_removes,
	struct save_states saves;
	setup_saves(&saves);
	assert(!save_states_remove(&saves, "PLAYER_2"));
	assert(save_states_remove(&saves, "PLAYER_3"));
	size_t n_writ;
	char *writ = saves_to_string(&saves, &n_writ);
	assert(strstr(writ, "PLAYER_1"));
	assert(!strstr(writ, "PLAYER_2"));
	save_states_destroy(&saves);
)

CTF_TEST(save_state_completed,
	struct save_states saves;
	setup_saves(&saves);
	struct save_state *p3 = save_states_add(&saves, "PLAYER_3");
	assert(!save_state_is_complete(p3, "LEVEL"));
	save_state_mark_complete(p3, "LEVEL");
	assert(save_state_is_complete(p3, "LEVEL"));
	size_t n_writ;
	char *writ = saves_to_string(&saves, &n_writ);
	assert(strstr(writ, "LEVEL"));
	save_states_destroy(&saves);
)

#endif /* CTF_TESTS_ENABLED */

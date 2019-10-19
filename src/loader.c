#include "loader.h"
#include "pixel.h"
#include "string.h"
#include "util.h"
#include "xalloc.h"
#include <stdlib.h>
#include <string.h>

void loader_init(struct loader *ldr, const char *root)
{
	ldr->menu_file = mid_cat(root, '/', "menu.json");
	ldr->txtrs_dir = mid_cat(root, '/', "textures");
	table_init(&ldr->txtrs, 16);
	ldr->ents_dir = mid_cat(root, '/', "ents");
	table_init(&ldr->ents, 16);
	ldr->maps_dir = mid_cat(root, '/', "maps");
	table_init(&ldr->maps, 16);
	logger_init(&ldr->log);
	ldr->empty_txtr = NULL;
}

static void **load(table *tab, const char *root, const char *name, FILE **file,
	struct logger *log)
{
	void **item = table_get(tab, name);
	*file = NULL;
	if (!item) {
		char *path = mid_cat(root, '/', name);
		*file = fopen(path, "r");
		if (*file) {
			logger_printf(log, LOGGER_INFO, "Loading %s\n", path);
			table_add(tab, name, NULL);
			item = table_get(tab, name); // TODO: more efficient?
		} else {
			logger_printf(log, LOGGER_ERROR,
				"Cannot load %s\n", path);
		}
		free(path);
	}
	return item;
}

// Load with .json suffix
static void **loadj(table *tab, const char *root, const char *name, FILE **file,
	struct logger *log)
{
	char *fname = mid_cat(name, '.', "json");
	void **loaded = load(tab, root, fname, file, log);
	if (!*file) free(fname);
	return loaded;
}

struct ent_type **loader_ent(struct loader *ldr, const char *name, FILE **file)
{
	return (struct ent_type **)loadj(&ldr->ents, ldr->ents_dir, name, file,
		&ldr->log);
}

struct map **loader_map(struct loader *ldr, const char *name, FILE **file)
{
	return (struct map **)loadj(&ldr->maps, ldr->maps_dir, name, file,
		&ldr->log);
}

d3d_texture **loader_texture(struct loader *ldr, const char *name, FILE **file)
{
	char *fname = str_dup(name);
	d3d_texture **loaded = (d3d_texture **)load(&ldr->txtrs, ldr->txtrs_dir,
		fname, file, &ldr->log);
	if (!*file) free(fname);
	return loaded;
}

const d3d_texture *loader_empty_texture(struct loader *ldr)
{
	if (!ldr->empty_txtr) {
		ldr->empty_txtr = d3d_new_texture(1, 1);
		*d3d_texture_get(ldr->empty_txtr, 0, 0) = EMPTY_PIXEL;
	}
	return ldr->empty_txtr;
}

FILE *loader_menu_file(struct loader *ldr)
{
	return fopen(ldr->menu_file, "r");
}

static int free_txtrs_callback(const char *key, void **val)
{
	free((char *)key);
	d3d_free_texture(*val);
	return 0;
}

static int free_ents_callback(const char *key, void **val)
{
	free((char *)key);
	ent_type_free(*val);
	return 0;
}

static int free_maps_callback(const char *key, void **val)
{
	free((char *)key);
	map_free(*val);
	return 0;
}

void loader_print_summary(struct loader *ldr)
{
	logger_printf(&ldr->log, LOGGER_INFO,
		"Load summary: %zu maps, %zu entity types, %zu textures\n",
		table_count(&ldr->maps),
		table_count(&ldr->ents),
		table_count(&ldr->txtrs));
}

struct logger *loader_logger(struct loader *ldr)
{
	return &ldr->log;
}

void loader_free(struct loader *ldr)
{
	free(ldr->menu_file);
	d3d_free_texture(ldr->empty_txtr);
	logger_free(&ldr->log);
	table_each(&ldr->txtrs, free_txtrs_callback);
	table_free(&ldr->txtrs);
	free(ldr->txtrs_dir);
	table_each(&ldr->ents, free_ents_callback);
	table_free(&ldr->ents);
	free(ldr->ents_dir);
	table_each(&ldr->maps, free_maps_callback);
	table_free(&ldr->maps);
	free(ldr->maps_dir);
}

#if CTF_TESTS_ENABLED

#	include "libctf.h"
#	include <assert.h>

CTF_TEST(loader_loads_only_once,
	FILE *file;
	struct loader ldr;
	loader_init(&ldr, "data");
	d3d_texture *empty = d3d_new_texture(0, 0);
	d3d_texture **loaded = loader_texture(&ldr, "empty", &file);
	*loaded = empty;
	assert(*loader_texture(&ldr, "empty", &file) == empty);
	loader_free(&ldr);
)

CTF_TEST(loader_nonexistent_gives_null,
	FILE *file;
	struct loader ldr;
	loader_init(&ldr, "data");
	assert(!loader_map(&ldr, "doesn't exist", &file));
	loader_free(&ldr);
)

#endif /* CTF_TESTS_ENABLED */

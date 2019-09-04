#ifndef ENTITY_H_
#define ENTITY_H_

#include "d3d.h"
#include "loader.h"
#include "table.h"
#include <stdbool.h>

struct ent_frame {
	// The texture displayed.
	const d3d_texture *txtr;
	// Time where frame is visible, in game ticks.
	long duration;
};

struct ent_type {
	// The allocated type name.
	char *name;
	// The width of the entity in blocks.
	double width;
	// The height of the entity in blocks.
	double height;
	// The transparent pixel in the frames, or -1 for none.
	int transparent;
	// The number of frames.
	size_t n_frames;
	// The allocated list of n_frames frames.
	struct ent_frame *frames;
	// entity to spawn on death.
	struct ent_type *death_spawn;
	// Ticks to stay alive, or -1 for forever.
	long lifetime;
};

// Load an entity with the name or use one previously loaded.
// Allocate the entity.
struct ent_type *load_ent_type(struct loader *ldr, const char *name);

// Allocate a string representation for debugging.
char *ent_type_to_string(const struct ent_type *ent);

// Free a loaded entity type. Does nothing when given NULL.
void ent_type_free(struct ent_type *ent);

struct ent {
	struct ent_type *type;
	d3d_sprite_s *sprite;
	long lifetime;
	size_t frame;
	long frame_duration;
};

// Initialize an entity. The sprite will be owned and manipulated by it.
void ent_init(struct ent *ent, struct ent_type *type, d3d_sprite_s *sprite,
	const d3d_vec_s *pos);

// Go forward a tick of the normal lifecycle. Does not move the entity.
void ent_tick(struct ent *ent);

// Move an entity in memory. Its sprite will move to the given location. The
// destinations will be overwritten and the source will become effectively
// uninitialized.
void ent_relocate(struct ent *ent, struct ent *to_ent, d3d_sprite_s *to_sprite);

// Returns whether or not the entity is dead and should be removed.
bool ent_is_dead(const struct ent *ent);

// Clean up entity resources. Does not free the pointer.
void ent_destroy(struct ent *ent);

#endif /* ENTITY_H_ */

#ifndef LOAD_TEXTURE_H_
#define LOAD_TEXTURE_H_

#include "d3d.h"
#include "loader.h"
#include "table.h"

// The key implanted in the txtrs table by load_textures which corresponds to
// an empty sprite texture. This will always be present.
#define EMPTY_TXTR_KEY ""

// Load a texture named as given or give one already loaded.
d3d_texture *load_texture(struct loader *ldr, const char *name);

#endif /* LOAD_TEXTURE_H_ */

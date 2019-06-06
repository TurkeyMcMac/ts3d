#ifndef LOAD_TEXTURE_H_
#define LOAD_TEXTURE_H_

#include "d3d.h"
#include "table.h"

// The key implanted in the txtrs table by load_textures which corresponds to
// an empty sprite texture. This will always be present.
#define EMPTY_TXTR_KEY ""

// Try to load a texture from a file path, returning NULL on system error.
d3d_texture *load_texture(const char *path);

// Try to load the textures from the directory at dirpath into the given
// uninitialized table (from allocated C strings to texture pointers), returning
// negative on system error or zero otherwise.
int load_textures(const char *dirpath, table *txtrs);

#endif /* LOAD_TEXTURE_H_ */

#ifndef LOAD_TEXTURE_H_
#define LOAD_TEXTURE_H_

#include "d3d.h"
#include "table.h"

#define EMPTY_TXTR_KEY ""

d3d_texture *load_texture(const char *path);

int load_textures(const char *dirpath, table *txtrs);

#endif /* LOAD_TEXTURE_H_ */

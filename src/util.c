#include "util.h"
#include "xalloc.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

char *str_dup(const char *str)
{
	size_t size = strlen(str) + 1;
	return memcpy(xmalloc(size), str, size);
}

int sbprintf(char * restrict str, size_t size, const char * restrict fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int writ = vsnprintf(str, size, fmt, va);
	if ((size_t)writ >= size && writ > 0) writ = size - 1;
	va_end(va);
	return writ;
}


d3d_direction flip_direction(d3d_direction dir)
{
	switch (dir) {
	case D3D_DNORTH: return D3D_DSOUTH;
	case D3D_DSOUTH: return D3D_DNORTH;
	case D3D_DWEST: return D3D_DEAST;
	case D3D_DEAST: return D3D_DWEST;
	case D3D_DUP: return D3D_DDOWN;
	case D3D_DDOWN: return D3D_DUP;
	default: return dir;
	}
}

char *mid_cat(const char *part1, int mid, const char *part2)
{
	char *cat;
	size_t len1, len2, len;
	len1 = strlen(part1);
	len2 = strlen(part2);
	len = len1 + 1 + len2;
	cat = xmalloc(len + 1);
	memcpy(cat, part1, len1);
	cat[len1] = mid;
	memcpy(cat + len1 + 1, part2, len2 + 1);
	return cat;
}

void move_direction(d3d_direction dir, size_t *x, size_t *y)
{
	switch (dir) {
	case D3D_DNORTH: --*y; break;
	case D3D_DSOUTH: ++*y; break;
	case D3D_DWEST: --*x; break;
	case D3D_DEAST: ++*x; break;
	default: break;
	}
}

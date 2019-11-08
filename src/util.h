#ifndef UTIL_H_
#define UTIL_H_

#include "d3d.h"
#include <stddef.h>

// Version of GNU __attribute__ which expands to nothing if attributes aren't
// supported. Only single parentheses are used.
#ifdef __GNUC__
#	define ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#else
#	define ATTRIBUTE(...)
#endif

// Duplicate a NUL-terminated string, including the terminator, returning an
// allocated copy.
char *str_dup(const char *str);

// Version of snprintf which returns size-1 if it exceeds the buffer length:
int sbprintf(char * restrict str, size_t size, const char * restrict fmt, ...)
	ATTRIBUTE(format(printf, 3, 4));

// Turn a cardinal direction 180 degrees, or swap between up and down.
d3d_direction flip_direction(d3d_direction dir);

// Concat parts 1 and 2 with mid inbetween. Return the concatenated.
// NOTE: recalculates lengths, not efficient.
char *mid_cat(const char *part1, int mid, const char *part2);

// Move x OR y in the direction dir. North is -y. South is +y. West is -x. East
// is +x. Underflow in x or y is NOT accounted for.
void move_direction(d3d_direction dir, size_t *x, size_t *y);

// Get the bit in bits at the index idx, starting from the least significant.
// Zero or one is returned.
#define bitat(bits, idx) ((bits) >> (idx) & 1)

// Number of elements in an array whose size and type is known at compile time.
#define ARRSIZE(array) (sizeof(array) / sizeof *(array))

// Clamp num within the range [min, max]. Return the clamped value. Arguments
// will be evaluated multiple times.
// Precondition: min <= max
#define CLAMP(num, min, max) \
	((num) < (min) ? (min) : ((num) > (max) ? (max) : (num)))

// The constant pi.
#define PI 3.14159265358979323846

// Evaluate the expression that has a numeric type. If it is less than 0, return
// -1. Otherwise, continue onward.
#define TRY(expr) if ((expr) >= 0) ; else return -1

// Wrap this around the name of a variable at its point of definition to
// suppress warnings by the compiler that it is unused.
#define UNUSED_VAR(var) var ATTRIBUTE(unused)

#endif /* UTIL_H_ */

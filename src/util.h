/*
 * Copyright (C) 2013-2014  Travis Brown (travisb@travisbrown.ca)
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * Miscellanious functions and definitions which are useful.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

/* This one borrowed from Linux.
 *
 * Used like so:
 *
 * struct subclass {
 * 	struct a a;
 * 	struct superclass super;
 * } object;
 *
 * struct subclass *s = container_of(&object->super, struct subclass, super);
 */
#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

/*
 * Returns the number of elements in a statically sized array
 */
#define ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))

/*
 * Return the ascii character code of the normal character converted into control-X form.
 */
#define CONTROL(x) ( (x) & ~((1 << 6) | (1 << 5)))

/*
 * Like strncmp, but where the first string is constant and only true on exact equality.
 */
#define CONST_STR_IS(const_a, b) (strncmp((const_a), (b), sizeof(const_a)) == 0)

void close_on_exec(int fd);

/*
 * A bitmap type of arbitrary, but fixed at compile time, length.
 * n is the number of bits minimum.
 */
#define BITMAP_DECLARE(n) \
	struct { \
		uint32_t data[(n)/32 + 1]; \
	}

/*
 * Returns the value of the bit in question. Returns -1 if n is out of
 * bounds.
 */
#define BITMAP_GETBIT(bitmap, n) \
	_bitmap_getbit((bitmap), (n), ARRAY_SIZE((bitmap)->data))
static inline int _bitmap_getbit(void *bitmap, int n, uint32_t num_elements) {
	uint32_t *data = bitmap;
	uint32_t segment;

	if (n > num_elements * 32)
		return -1;

	segment = data[n / 32];
	return (segment >> (n % 32)) & 0x1;
}

/*
 * Set the given bit to the given truth value
 */
#define BITMAP_SETBIT(bitmap, n, val) \
	_bitmap_setbit((bitmap), (n), (val), ARRAY_SIZE((bitmap)->data))
static inline void _bitmap_setbit(void *bitmap, int n, uint64_t val, uint32_t num_elements) {
	uint32_t *data = bitmap;
	uint32_t *segment;
	uint32_t mask;

	if (n > num_elements * 32)
		return;

	segment = &data[n / 32];
	mask = ~(1 << (n % 32));

	if (val)
		val = ~0;
	else
		val = 0;

	*segment = (*segment & mask) | (val & ~mask);
}

#endif

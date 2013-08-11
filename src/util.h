/*
 * Copyright (C) 2013  Travis Brown (travisb@travisbrown.ca)
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

#endif

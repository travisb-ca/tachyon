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
 * Structure used to store the commandline options
 */
#ifndef OPTIONS_H
#define OPTIONS_H

extern struct cmd_options {
	int predict; /* Should character prediction be performed ? */
	struct {
		char meta; /* The key combination which accesses the meta terminal functionality */
		char buffer_create; /* The key command which creates a new buffer */
	} keys;
} cmd_options;

#endif

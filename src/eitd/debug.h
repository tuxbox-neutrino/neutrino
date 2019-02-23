/*
 * debug.h, Debug tools (sectionsd) - d-box2 linux project
 *
 * (C) 2001 by fnbrd (fnbrd@gmx.de),
 *     2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __sectionsd__debug_h__
#define __sectionsd__debug_h__


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>

extern int sections_debug;

enum
{
	DEBUG_ERROR	, // 0
	DEBUG_NORMAL, // 1
	DEBUG_INFO	, // 2
	DEBUG_DEBUG	, // 3

	DEBUG_MODES	  // 4 count of available modes
};

/* dont add \n when using this */
#define debug(debuglevel, fmt, args...) do { \
	if (sections_debug >= debuglevel) { \
		FILE *fs = (debuglevel == DEBUG_ERROR ? stderr : stdout); \
		printdate_ms(fs); \
		fprintf(fs, "[sectionsd] " fmt "\n", ## args); \
		fflush(fs); \
	}\
} while (0)

/* dont add \n when using this */
#define debug_colored(debuglevel, fmt, args...) do { \
	if (sections_debug >= debuglevel) { \
		FILE *fs = (debuglevel == DEBUG_ERROR ? stderr : stdout); \
		fprintf(fs, "%c[%d;%d;%dm", 0x1B, 1, 31, 40);\
		printdate_ms(fs); \
		fprintf(fs, "[sectionsd] " fmt, ## args); \
		fprintf(fs, "%c[%dm\n", 0x1B, 0); \
		fflush(fs); \
	}\
}  while (0)

void printdate_ms(FILE* f);
void showProfiling( std::string text );

#endif /* __sectionsd__debug_h__ */


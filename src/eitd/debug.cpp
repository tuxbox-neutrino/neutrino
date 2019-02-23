/*
 * debug.cpp, Debug tools (sectionsd) - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
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
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "debug.h"

int sections_debug = DEBUG_NORMAL;

void printdate_ms(FILE *f) {
	timeval now;
	gettimeofday(&now, NULL);
	struct tm *tm = localtime(&now.tv_sec);
	/* use strftime for that? */
	fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d.%03ld ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec/1000);
}

static int64_t last_profile_call;

void showProfiling( std::string text )
{
	struct timeval tv;

	gettimeofday( &tv, NULL );
	int64_t now = (int64_t) tv.tv_usec + (int64_t)((int64_t) tv.tv_sec * (int64_t) 1000000);

	int64_t tmp = now - last_profile_call;
	printf("--> '%s' %lld.%03lld\n", text.c_str(), tmp / 1000LL, tmp % 1000LL);
	last_profile_call = now;
}

/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/daemons/sectionsd/debug.cpp,v 1.3 2008/06/15 10:19:38 seife Exp $
 *
 * Debug tools (sectionsd) - d-box2 linux project
 *
 * (C) 2003 by thegoodguy <thegoodguy@berlios.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

bool sections_debug;

void printdate_ms(FILE *f) {
	timeval now;
	gettimeofday(&now, NULL);
	struct tm *tm = localtime(&now.tv_sec);
	/* use strftime for that? */
	fprintf(f, "%02d:%02d:%02d.%03ld ", tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec/1000);
}

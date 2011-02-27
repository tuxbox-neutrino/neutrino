/*
 * $Id: debug.h,v 1.7 2003/04/30 04:39:03 obi Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __zapit_debug_h__
#define __zapit_debug_h__


#include <cerrno>
#include <cstdio>
#include <cstring>


#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#define DEBUG	1
#endif

/*
 * Suppress warnings when GCC is in -pedantic mode and not -std=c99
 */
#if (__GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
#pragma GCC system_header
#endif

/* zapit.cpp */
extern int zapit_debug;

#define DBG(fmt, args...)					\
	do {							\
		if (zapit_debug)					\
			fprintf(stdout, "[%s:%s:%d] " fmt,	\
				strrchr(__FILE__, '/'), __FUNCTION__,		\
				__LINE__ , ## args);		\
	} while (0)

#define ERROR(str)						\
	do {							\
		fprintf(stderr, "[%s:%s:%d] %s: %s\n",		\
			strrchr(__FILE__, '/'), __FUNCTION__,		\
			__LINE__, str, strerror(errno));	\
	} while (0)

#ifdef DEBUG

#define INFO(fmt, args...)					\
	do {							\
		fprintf(stdout, "[%s:%s:%d] " fmt "\n",		\
			strrchr(__FILE__, '/'), __FUNCTION__,		\
			__LINE__ , ## args);			\
	} while (0)

#define WARN(fmt, args...)					\
	do {							\
		fprintf(stderr, "[%s:%s:%d] " fmt "\n",		\
			strrchr(__FILE__, '/'), __FUNCTION__,		\
			__LINE__ , ## args);			\
	} while (0)

#else /* DEBUG */

#define INFO(fmt, args...)
#define WARN(fmt, args...)

#endif /* DEBUG */

#define fop(cmd, args...) ({					\
	int _r;							\
	if (fd >= 0) { 						\
		if ((_r = ::cmd(fd, args)) < 0)			\
			ERROR(#cmd"(fd, "#args")");		\
		else if (zapit_debug)					\
			INFO(#cmd"(fd, "#args")");		\
	}							\
	else { _r = fd; } 					\
	_r;							\
})

#define quiet_fop(cmd, args...) ({				\
	int _r;							\
	if (fd >= 0) { _r = ::cmd(fd, args); }			\
	else { _r = fd; } 					\
	_r;							\
})

#define TIMER_START()						\
	static struct timeval tv, tv2;				\
	static unsigned int msec;				\
	gettimeofday(&tv, NULL)

#define TIMER_STOP(label)								\
	gettimeofday(&tv2, NULL);							\
	msec = ((tv2.tv_sec - tv.tv_sec) * 1000) + ((tv2.tv_usec - tv.tv_usec) / 1000);	\
	printf("%s: %u msec\n", label, msec)

#define TIMER_STOP_SEC(label)					\
	gettimeofday(&tv2, NULL);				\
	msec = tv2.tv_sec - tv.tv_sec;				\
	printf("%s: %d sec\n", label, msec)

#endif /* __zapit_debug_h__ */

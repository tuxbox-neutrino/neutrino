/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Copyright (C) 2017, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __COMPATIBILITY_H__
#define __COMPATIBILITY_H__


#if !defined __UCLIBC__ || ((__UCLIBC_MAJOR__ >= 1) && (__UCLIBC_MINOR__ >= 0) && (__UCLIBC_SUBLEVEL__ >= 10))
#define comp_malloc_stats(a) malloc_stats()
#else
#define comp_malloc_stats(a) malloc_stats(a)
#endif


#endif // __COMPATIBILITY_H__

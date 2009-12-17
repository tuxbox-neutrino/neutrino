/*
 * $Header: /cvs/tuxbox/apps/tuxbox/neutrino/src/driver/screen_max.h,v 1.2 2009/12/15 09:42:59 dbt Exp $
 *
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


#ifndef  __SCREEN_MAX__
#define  __SCREEN_MAX__


int w_max (int w_size, int w_add);
int h_max (int h_size, int h_add);
int getScreenStartX(int width);
int getScreenStartY(int height);


#endif



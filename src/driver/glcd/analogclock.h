/*
        LCD-Daemon  -   DBoxII-Project

        Copyright (C) 2001 Steffen Hehn 'McClean'
        Homepage: http://dbox.cyberphoria.org/



        License: GPL

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "glcd.h"

#define LCDDIR_VAR "/usr/share/tuxbox/neutrino/icons"

#define LCD_NUMBER_OF_FILES 3

void InitAnalogClock();
void RenderClock(int x, int y);
void RenderHands(int hour, int min, int sec, int posx, int posy, int hour_size, int min_size, int sec_size);
void ShowAnalogClock(int hour, int min, int sec, int x, int y);

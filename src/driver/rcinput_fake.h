/*
	Neutrino-GUI  -   DBoxII-Project

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

#ifndef __rcinput_fake_h__
#define __rcinput_fake_h__

#ifndef KEY_NONEXISTING
#define KEY_NONEXISTING  0x0
#endif

#ifndef KEY_OK
#define KEY_OK           0x160
#endif

#ifndef KEY_RED
#define KEY_RED          0x18e
#endif

#ifndef KEY_GREEN
#define KEY_GREEN        0x18f
#endif

#ifndef KEY_YELLOW
#define KEY_YELLOW       0x190
#endif

#ifndef KEY_BLUE
#define KEY_BLUE         0x191
#endif

#ifndef KEY_GAMES
#define KEY_GAMES        0x1a1
#endif

#ifndef KEY_TOPLEFT
#define KEY_TOPLEFT      0x1a2
#endif

#ifndef KEY_TOPRIGHT
#define KEY_TOPRIGHT     0x1a3
#endif

#ifndef KEY_BOTTOMLEFT
#define KEY_BOTTOMLEFT   0x1a4
#endif

#ifndef KEY_BOTTOMRIGHT
#define KEY_BOTTOMRIGHT  0x1a5
#endif

#define KEY_POWERON	KEY_FN_F1
#define KEY_POWEROFF	KEY_FN_F2
#define KEY_STANDBYON	KEY_FN_F3
#define KEY_STANDBYOFF	KEY_FN_F4
#define KEY_MUTEON	KEY_FN_F5
#define KEY_MUTEOFF	KEY_FN_F6
#define KEY_ANALOGON	KEY_FN_F7
#define KEY_ANALOGOFF	KEY_FN_F8

#define KEY_TTTV	KEY_FN_1
#define KEY_TTZOOM	KEY_FN_2
#define KEY_REVEAL	KEY_FN_D

// only defined in newer kernels / headers...

#ifndef KEY_ZOOMIN
#define KEY_ZOOMIN	KEY_FN_E
#endif

#ifndef KEY_ZOOMOUT
#define KEY_ZOOMOUT	KEY_FN_F
#endif

/* still available, even in 2.6.12:
	#define KEY_FN_S
	#define KEY_FN_B
*/

#endif

/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012, 2013, Thilo Graf 'dbt'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to the
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/

#ifndef __CC_TYPES__
#define __CC_TYPES__

#include <driver/framebuffer.h>
#include <system/localize.h>

///cc item types
typedef enum
{
	CC_ITEMTYPE_BASE,
	CC_ITEMTYPE_PICTURE,
	CC_ITEMTYPE_TEXT,
	CC_ITEMTYPE_TEXT_INFOBOX,
	CC_ITEMTYPE_SHAPE_SQUARE,
	CC_ITEMTYPE_SHAPE_CIRCLE,
	CC_ITEMTYPE_PIP,
	CC_ITEMTYPE_FRM,
	CC_ITEMTYPE_FRM_HEADER,
	CC_ITEMTYPE_FRM_ICONFORM,
	CC_ITEMTYPE_FRM_WINDOW,
	CC_ITEMTYPE_LABEL,
	CC_ITEMTYPE_PROGRESSBAR,
	CC_ITEMTYPE_BUTTON,
	CC_ITEMTYPE_BUTTON_RED,
	CC_ITEMTYPE_BUTTON_GREEN,
	CC_ITEMTYPE_BUTTON_YELLOW,
	CC_ITEMTYPE_BUTTON_BLUE,

	CC_ITEMTYPES
}CC_ITEMTYPES_T;

//required typedefs
typedef struct comp_fbdata_t
{
	int fbdata_type;
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t color;
	int r;
	int frame_thickness;
	fb_pixel_t* pixbuf;
	void * data;
} comp_fbdata_struct_t;

//fb data object types
typedef enum
{
	CC_FBDATA_TYPE_BGSCREEN,
	CC_FBDATA_TYPE_BOX,
	CC_FBDATA_TYPE_FRAME,
	CC_FBDATA_TYPE_LINE,
	CC_FBDATA_TYPE_BACKGROUND,

	CC_FBDATA_TYPES
}FBDATA_TYPES;

typedef struct comp_screen_data_t
{
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t* pixbuf;
} comp_screen_data_struct_t;

//align types
enum
{
	CC_ALIGN_RIGHT 		= 1,
	CC_ALIGN_LEFT 		= 2,
	CC_ALIGN_TOP 		= 4,
	CC_ALIGN_BOTTOM 	= 8,
	CC_ALIGN_HOR_CENTER	= 16,
	CC_ALIGN_VER_CENTER	= 32
};

enum
{
	CC_ITEMBOX_ICON,
	CC_ITEMBOX_PICTURE,
	CC_ITEMBOX_TEXT,
	CC_ITEMBOX_CLOCK
};

typedef struct comp_element_data_t
{
	int		type;
	int		align;
	std::string	element;
	int		x;
	int		y;
	int		width;
	int		height;
	void*		handler1;
	void*		handler2;
}comp_element_data_struct_t;


#define CC_WIDTH_MIN		16
#define CC_HEIGHT_MIN		16
#define CC_SHADOW_ON 		true
#define CC_SHADOW_OFF 		false
#define CC_SAVE_SCREEN_YES 	true
#define CC_SAVE_SCREEN_NO 	false

#define CC_NO_INDEX	 	-1



#endif

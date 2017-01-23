/*
	Based up Neutrino-GUI - Tuxbox-Project 
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2014, Thilo Graf 'dbt'

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
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <gui/color_custom.h>

class CComponentsForm;
class CComponentsScrollBar;

///cc item types
typedef enum
{
	CC_ITEMTYPE_GENERIC,
	CC_ITEMTYPE_ITEM,
	CC_ITEMTYPE_PICTURE,
	CC_ITEMTYPE_TEXT,
	CC_ITEMTYPE_TEXT_INFOBOX,
	CC_ITEMTYPE_SHAPE_SQUARE,
	CC_ITEMTYPE_SHAPE_CIRCLE,
	CC_ITEMTYPE_PIP,
	CC_ITEMTYPE_FRM,
	CC_ITEMTYPE_FRM_CHAIN,
	CC_ITEMTYPE_FRM_CLOCK,
	CC_ITEMTYPE_FRM_HEADER,
	CC_ITEMTYPE_FOOTER,
	CC_ITEMTYPE_FRM_ICONFORM,
	CC_ITEMTYPE_FRM_WINDOW,
	CC_ITEMTYPE_FRM_EXT_TEXT,
	CC_ITEMTYPE_LABEL,
	CC_ITEMTYPE_PROGRESSBAR,
	CC_ITEMTYPE_BUTTON,
	CC_ITEMTYPE_BUTTON_RED,
	CC_ITEMTYPE_BUTTON_GREEN,
	CC_ITEMTYPE_BUTTON_YELLOW,
	CC_ITEMTYPE_BUTTON_BLUE,
	CC_ITEMTYPE_SLIDER,
	CC_ITEMTYPE_FRM_SCROLLBAR,
	CC_ITEMTYPE_FRM_SIGNALBAR,

	CC_ITEMTYPES
}CC_ITEMTYPES_T;

//required typedefs
typedef struct cc_fbdata_t
{
	bool enabled;
	int fbdata_type;
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t color;
	int r;
	int rtype;
	int frame_thickness;
	fb_pixel_t* pixbuf;
	gradientData_t *gradient_data;
	void * data;
	bool is_painted;
} cc_fbdata_struct_t;

//fb data object layer types
typedef enum
{
	CC_FBDATA_TYPE_BGSCREEN 	= 1,
	CC_FBDATA_TYPE_BOX		= 2,
	CC_FBDATA_TYPE_SHADOW_BOX 	= 4,
	CC_FBDATA_TYPE_FRAME		= 8,
	CC_FBDATA_TYPE_BACKGROUND 	= 16,

	CC_FBDATA_TYPES			= 32
}FBDATA_TYPES;

//fb color gradient types
typedef enum
{
	CC_COLGRAD_OFF, 		//no gradient
	CC_COLGRAD_LIGHT_2_DARK,	//normal one color
	CC_COLGRAD_DARK_2_LIGHT,	//changed
	CC_COLGRAD_COL_A_2_COL_B,	//gradient from color A to color B
	CC_COLGRAD_COL_B_2_COL_A,	//gradient from color B to color A
	CC_COLGRAD_COL_LIGHT_DARK_LIGHT,//gradient from color A to B to A
	CC_COLGRAD_COL_DARK_LIGHT_DARK,	//gradient from color B to A to B

	CC_COLGRAD_TYPES
}COLOR_GRADIENT_TYPES;

typedef struct cc_screen_data_t
{
	int x;
	int y;
	int dx;
	int dy;
	fb_pixel_t* pixbuf;
} cc_screen_data_struct_t;

//combination of rc messages with related icon
typedef struct msg_list_t
{
	neutrino_msg_t 	directKey;
	const char* 	icon;
} msg_list_struct_t;

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

//item centering modes, see also CComponentsItem::setCenterPos()
enum
{
	CC_ALONG_X 		= 1,
	CC_ALONG_Y 		= 2
};

typedef struct cc_element_data_t
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
}cc_element_data_struct_t;

//text label types
typedef struct cc_locale_ext_txt_t
{
	neutrino_locale_t label_text;
	neutrino_locale_t text;
	Font* font;
} cc_locale_ext_txt_struct_t;

typedef struct cc_string_ext_txt_t
{
	std::string label_text;
	std::string text;
	Font* font;
} cc_string_ext_txt_struct_t;

#define CC_WIDTH_MIN		16
#define CC_HEIGHT_MIN		16

#define CC_SHADOW_OFF 			0x0
#define CC_SHADOW_RIGHT 		0x2
#define CC_SHADOW_BOTTOM 		0x4
#define CC_SHADOW_CORNER_BOTTOM_LEFT	0x8
#define CC_SHADOW_CORNER_BOTTOM_RIGHT	0x10
#define CC_SHADOW_CORNER_TOP_RIGHT	0x20
#define CC_SHADOW_ON 			CC_SHADOW_RIGHT | CC_SHADOW_BOTTOM | CC_SHADOW_CORNER_BOTTOM_LEFT | CC_SHADOW_CORNER_BOTTOM_RIGHT | CC_SHADOW_CORNER_TOP_RIGHT

#define CC_SAVE_SCREEN_YES 	true
#define CC_SAVE_SCREEN_NO 	false

#define CC_NO_INDEX	 	-1

///predefined parameters for auto positionizing of embedded items inside a parent form
///CC_APPEND used for x or y position or booth. An item with this parameter will paint automatically arranged side by side
#define CC_APPEND		-1
///CC CENTERED used for x or y position or booth. An item with this parameter will paint automatically centered
#define CC_CENTERED		-2



#endif

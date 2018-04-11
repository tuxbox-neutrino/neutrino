/*
	Based up Neutrino-GUI - Tuxbox-Project
 
	Copyright (C) 2001 Steffen Hehn 'McClean'
	Copyright (C) 2018 Thilo Graf

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


#ifndef __colorchooser__
#define __colorchooser__

#include <system/localize.h>
#include <gui/widget/menue.h>

#include <string>

enum {
	VALUE_R = 0, // red
	VALUE_G = 1, // green
	VALUE_B = 2, // blue
	VALUE_A = 3, // alpha

	VALUES
};

class CFrameBuffer;
class CColorChooser : public CMenuTarget
{
	private:
		CFrameBuffer *frameBuffer;
		int x;
		int y;
		int width;
		int height;
		int header_height;
		int item_height;
		int text_width;
		int bar_width;
		int bar_offset;
		int bar_full;
		int slider_width;
		int slider_step;
		int preview_x;
		int preview_y;
		int preview_w;
		int preview_h;

		int chooser_gradient;

		unsigned char * value[VALUES];

		neutrino_locale_t name;

		CChangeObserver* observer;
		Font	* font;

		void Init();
		void paint();
		void setColor();
		void paintSlider(int x, int y, unsigned char *spos, const neutrino_locale_t text, const char * const iconname, const bool selected);

	public:
		CColorChooser(const neutrino_locale_t Name, unsigned char *R, unsigned char *G, unsigned char *B, unsigned char* A, CChangeObserver* Observer = NULL);

		void hide();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		fb_pixel_t getColor(void);
		enum
		{
			gradient_none,
			gradient_head_body,
			gradient_head_text
		};
		void setGradient(int gradient = gradient_none) { chooser_gradient = gradient; };
};

#endif

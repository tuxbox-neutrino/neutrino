/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Class for epg window navigation bar.
	Copyright (C) 2017-2018, Thilo Graf 'dbt'

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "navibar.h"


using namespace std;


CNaviBar::CNaviBar(	const int& x_pos,
			const int& y_pos,
			const int& dx,
			const int& dy,
			CComponentsForm* parent,
			int shadow_mode,
			fb_pixel_t& color_frame,
			fb_pixel_t& color_body,
			fb_pixel_t& color_shadow)
			: CComponentsFrmChain(	x_pos, y_pos, dx, dy,
						NULL,
						CC_DIR_X,
						parent,
						shadow_mode,
						color_frame,
						color_body,
						color_shadow)
{
	setCornerType(CORNER_NONE);
	enableColBodyGradient(g_settings.theme.infobar_gradient_bottom,COL_MENUFOOT_PLUS_0,g_settings.theme.infobar_gradient_bottom_direction);
	set2ndColor(COL_MENUCONTENT_PLUS_0);

	nb_lpic = nb_rpic 	= NULL;
	nb_lText = nb_rText 	= NULL;
	nb_font			= g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE];;
	nb_lpic_enable = nb_rpic_enable = false;
	nb_l_text = nb_r_text = string();

	initCCItems();
}

void CNaviBar::initCCItems()
{
	int x_off 	= OFFSET_INNER_MID;
	int mid_width 	= width * 40 / 100; // 40%
	int side_width 	= ((width - mid_width) / 2) - (2 * x_off);
	int h_text 	= height;
	int icon_h, icon_w;

	// init left arrow
	if (!nb_lpic){
		nb_lpic = new CComponentsPicture(x_off,CC_CENTERED, CFrameBuffer::getInstance()->getIconPath(NEUTRINO_ICON_BUTTON_LEFT));
		nb_lpic->getRealSize(&icon_h, &icon_w);
		if ((icon_h + 2*OFFSET_INNER_MIN) > (height + 2*OFFSET_INNER_MIN))
			nb_lpic->setHeight(height - 2*OFFSET_INNER_MIN, true);
		nb_lpic->doPaintBg(false);
		this->addCCItem(nb_lpic);
		nb_lpic->enableSaveBg();
	}
	nb_lpic->allowPaint(nb_lpic_enable);

	// init right arrow
	if (!nb_rpic){
		nb_rpic = new CComponentsPicture(0,CC_CENTERED, CFrameBuffer::getInstance()->getIconPath(NEUTRINO_ICON_BUTTON_RIGHT));
		nb_rpic->getRealSize(&icon_h, &icon_w);
		if ((icon_h + 2*OFFSET_INNER_MIN) > (height + 2*OFFSET_INNER_MIN))
			nb_rpic->setHeight(height - 2*OFFSET_INNER_MIN, true);
		nb_rpic->doPaintBg(false);
		this->addCCItem(nb_rpic);
		nb_rpic->enableSaveBg();
		int x_pos = width - nb_rpic->getWidth() - x_off;
		nb_rpic->setXPos(x_pos);
	}
	nb_rpic->allowPaint(nb_rpic_enable);

	// init text left
	if (!nb_lText){
		nb_lText = new CComponentsText(x_off + nb_lpic->getWidth() + x_off, CC_CENTERED, side_width, h_text, "", CTextBox::NO_AUTO_LINEBREAK, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE], CComponentsText::FONT_STYLE_REGULAR, this, CC_SHADOW_OFF, COL_MENUHEAD_TEXT);
		nb_lText->doPaintBg(false);
		nb_lText->enableSaveBg();
	}
	nb_lText->setText(nb_l_text, CTextBox::NO_AUTO_LINEBREAK, nb_font, COL_MENUHEAD_TEXT, CComponentsText::FONT_STYLE_REGULAR);

	// init text right
	if (!nb_rText){
		nb_rText = new CComponentsText(0, CC_CENTERED, side_width, h_text, "", CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT, g_Font[SNeutrinoSettings::FONT_TYPE_EPG_DATE], CComponentsText::FONT_STYLE_REGULAR, this, CC_SHADOW_OFF, COL_MENUHEAD_TEXT);
		nb_rText->doPaintBg(false);
		nb_rText->enableSaveBg();
	}
	nb_rText->setText(nb_r_text, CTextBox::NO_AUTO_LINEBREAK | CTextBox::RIGHT, nb_font);
	nb_rText->setXPos(nb_rpic->getXPos() - x_off - nb_rText->getWidth());
}


void CNaviBar::paint(bool do_save_bg)
{
	hideCCItems();
	nb_lText->hide();
	nb_rText->hide();
	CComponentsFrmChain::paint(do_save_bg);
}

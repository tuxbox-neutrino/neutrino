/*
	(C)2012-2013 by martii

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/psisetup.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>

#include <gui/color.h>

#include <gui/widget/msgbox.h>
#include <gui/widget/icons.h>
#include <driver/screen_max.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <global.h>
#include <neutrino.h>

#include <hardware/video.h>

extern cVideo * videoDecoder;

struct PSI_list
{
	int control;
	const neutrino_locale_t loc;
	bool selected;
	CProgressBar *scale;
	unsigned char value;
	unsigned char value_old;
	int x;
	int y;
	int xLoc;
	int yLoc;
	int xBox;
	int yBox;
};

#define PSI_SCALE_COUNT 5
static PSI_list
	psi_list[PSI_SCALE_COUNT] = {
#define PSI_CONTRAST 0
	{ VIDEO_CONTROL_CONTRAST, LOCALE_VIDEOMENU_PSI_CONTRAST, true, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
#define PSI_SATURATION 1
	, { VIDEO_CONTROL_SATURATION, LOCALE_VIDEOMENU_PSI_SATURATION, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
#define PSI_BRIGHTNESS 2
	, { VIDEO_CONTROL_BRIGHTNESS, LOCALE_VIDEOMENU_PSI_BRIGHTNESS, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
#define PSI_TINT 3
	, { VIDEO_CONTROL_HUE, LOCALE_VIDEOMENU_PSI_TINT, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
#define PSI_RESET 4
	, { -1, LOCALE_VIDEOMENU_PSI_RESET, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0 }
};

#define SLIDERWIDTH CFrameBuffer::getInstance()->scale2Res(200)
#define SLIDERHEIGHT CFrameBuffer::getInstance()->scale2Res(15)
#define LOCGAP OFFSET_INNER_MID

CPSISetup::CPSISetup (const neutrino_locale_t Name)
{
	frameBuffer = CFrameBuffer::getInstance ();
	name = Name;
	selected = 0;

	for (int i = 0; i < PSI_RESET; i++)
		psi_list[i].scale = new CProgressBar();

	psi_list[PSI_CONTRAST].value = g_settings.psi_contrast;
	psi_list[PSI_SATURATION].value = g_settings.psi_saturation;
	psi_list[PSI_BRIGHTNESS].value = g_settings.psi_brightness;
	psi_list[PSI_TINT].value = g_settings.psi_tint;

	for (int i = 0; i < PSI_RESET; i++)
		videoDecoder->SetControl (psi_list[i].control, psi_list[i].value);

	needsBlit = true;
}

void CPSISetup::blankScreen(bool blank) {
	for (int i = 0; i < PSI_RESET; i++)
		videoDecoder->SetControl(psi_list[i].control, blank ? 0 : psi_list[i].value);
}

int CPSISetup::exec (CMenuTarget * parent, const std::string &)
{
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	locWidth = 0;
	for (int i = 0; i < PSI_SCALE_COUNT; i++)
	{
		int w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (g_Locale->getText(psi_list[i].loc)) + OFFSET_INNER_SMALL;
		if (w > locWidth)
			locWidth = w;
	}
	locHeight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight ();
	if (locHeight < SLIDERHEIGHT)
		locHeight = SLIDERHEIGHT + OFFSET_INNER_SMALL;

	sliderOffset = (locHeight - SLIDERHEIGHT) >> 1;

	//            [ SLIDERWIDTH ][5][locwidth    ]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight][XXXXXXXXXXXXX]   [XXXXXXXXXXXX]
	// [locHeight]                  [XXXXXXXXXXXX]

	dx = SLIDERWIDTH + LOCGAP + locWidth;
	dy = PSI_SCALE_COUNT * locHeight + (PSI_SCALE_COUNT - 1) * 2;

	x = frameBuffer->getScreenX() + ((frameBuffer->getScreenWidth() - dx) >> 1);
	y = frameBuffer->getScreenY() + ((frameBuffer->getScreenHeight() - dy) >> 1);

	int res = menu_return::RETURN_REPAINT;
	if (parent)
		parent->hide ();

	for (int i = 0; i < PSI_SCALE_COUNT; i++)
	{
		psi_list[i].value = psi_list[i].value_old = 128;
		psi_list[i].x = x;
		psi_list[i].y = y + locHeight * i + i * 2;
		psi_list[i].xBox = psi_list[i].x + SLIDERWIDTH + LOCGAP;
		psi_list[i].yBox = psi_list[i].y;
		psi_list[i].xLoc = psi_list[i].x + SLIDERWIDTH + LOCGAP + OFFSET_INNER_SMALL;
		psi_list[i].yLoc = psi_list[i].y + locHeight - 1;
	}

	for (int i = 0; i < PSI_RESET; i++)
		psi_list[i].scale->reset ();

	psi_list[PSI_CONTRAST].value = g_settings.psi_contrast;
	psi_list[PSI_SATURATION].value = g_settings.psi_saturation;
	psi_list[PSI_BRIGHTNESS].value = g_settings.psi_brightness;
	psi_list[PSI_TINT].value = g_settings.psi_tint;

	for (int i = 0; i < PSI_RESET; i++)
		psi_list[i].value_old = psi_list[i].value;

	paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] ? g_settings.timing[SNeutrinoSettings::TIMING_MENU] : 0xffff);
	bool loop = true;
	while (loop)
	{
		if(needsBlit) {
			frameBuffer->blit();
			needsBlit = false;
	}
	g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);
	if ( msg <= CRCInput::RC_MaxRC )
		timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] ? g_settings.timing[SNeutrinoSettings::TIMING_MENU] : 0xffff);
	int i;
	int direction = 1; // down
	switch (msg)
	{
		case CRCInput::RC_up:
			direction = -1;
			/* fall through */
		case CRCInput::RC_down:
			if (selected + direction > -1 && selected + direction < PSI_RESET)
			{
				psi_list[selected].selected = false;
				paintSlider (selected);
				selected += direction;
				psi_list[selected].selected = true;
				paintSlider (selected);
			}
			break;
		case CRCInput::RC_right:
			if (selected < PSI_RESET && psi_list[selected].value < 255)
			{
				int val = psi_list[selected].value + g_settings.psi_step;
				psi_list[selected].value = (val > 255) ? 255 : val;
				paintSlider (selected);
				videoDecoder->SetControl(psi_list[selected].control, psi_list[selected].value);
			}
			break;
		case CRCInput::RC_left:
			if (selected < PSI_RESET && psi_list[selected].value > 0)
			{
				int val = psi_list[selected].value - g_settings.psi_step;
				psi_list[selected].value = (val < 0) ? 0 : val;
				paintSlider (selected);
				videoDecoder->SetControl(psi_list[selected].control, psi_list[selected].value);
			}
			break;
		case CRCInput::RC_home:	// exit -> revert changes
			for (i = 0; (i < PSI_RESET) && (psi_list[i].value == psi_list[i].value_old); i++);
			if (ShowMsg(name, LOCALE_MESSAGEBOX_ACCEPT, CMsgBox::mbrYes, CMsgBox::mbYes | CMsgBox::mbCancel) == CMsgBox::mbrCancel)
				for (i = 0; i < PSI_RESET; i++)
				{
					psi_list[i].value = psi_list[i].value_old;
					videoDecoder->SetControl(psi_list[selected].control, psi_list[selected].value);
				}
			/* fall through */
		case CRCInput::RC_ok:
			if (selected != PSI_RESET)
			{
				loop = false;
				g_settings.psi_contrast = psi_list[PSI_CONTRAST].value;
				g_settings.psi_saturation = psi_list[PSI_SATURATION].value;
				g_settings.psi_brightness = psi_list[PSI_BRIGHTNESS].value;
				g_settings.psi_tint = psi_list[PSI_TINT].value;
				break;
			}
			/* fall through */
		case CRCInput::RC_red:
			for (i = 0; i < PSI_RESET; i++)
			{
				psi_list[i].value = 128;
				videoDecoder->SetControl(psi_list[i].control, psi_list[i].value);
				paintSlider (i);
			}
			break;
		default:
			;
		}
	}

	hide ();

	return res;
}

void CPSISetup::hide ()
{
	frameBuffer->paintBackgroundBoxRel (x, y, dx, dy);
	frameBuffer->blit();
}

void CPSISetup::paint ()
{
	for (int i = 0; i < PSI_SCALE_COUNT; i++)
		paintSlider (i);
}

void CPSISetup::paintSlider (int i)
{
	Font *f = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	fb_pixel_t fg_col[] = { COL_MENUCONTENTINACTIVE_TEXT, COL_MENUCONTENT_TEXT };

	if (i < PSI_RESET)
	{
		psi_list[i].scale->setProgress(psi_list[i].x, psi_list[i].y + sliderOffset, SLIDERWIDTH, SLIDERHEIGHT, psi_list[i].value, 255);
		psi_list[i].scale->paint();
		f->RenderString (psi_list[i].xLoc, psi_list[i].yLoc, locWidth, g_Locale->getText(psi_list[i].loc), fg_col[psi_list[i].selected]);
	}
	else
	{
		int fh = f->getHeight();
		frameBuffer->paintIcon (NEUTRINO_ICON_BUTTON_RED, psi_list[i].x, psi_list[i].yLoc - fh + fh/4, 0, (6 * fh)/8);
		f->RenderString (psi_list[i].xLoc, psi_list[i].yLoc, locWidth, g_Locale->getText(psi_list[i].loc), COL_MENUCONTENT_TEXT);
	}
	needsBlit = true;
}

bool CPSISetup::changeNotify (const neutrino_locale_t OptionName, void *Data)
{
	for (int i = 0; i < PSI_RESET; i++)
		if (OptionName == psi_list[i].loc)
		{
			psi_list[i].value = *((int *) Data);
			videoDecoder->SetControl(psi_list[i].control, psi_list[i].value);
			return true;
		}
	return false;
}

static CPSISetup *inst = NULL;

CPSISetup *CPSISetup::getInstance()
{
	if (!inst)
		inst = new CPSISetup(LOCALE_VIDEOMENU_PSI);
	return inst;
}

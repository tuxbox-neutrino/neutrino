#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/color.h>
#include <gui/widget/msgbox.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <system/settings.h>

#include <global.h>
#include <neutrino.h>
#include <gui/pipsetup.h>
#include <hardware/video.h>

#define PERCENT 5
#define XMOVE 5
#define YMOVE 5

#if ENABLE_PIP

extern cVideo *pipVideoDecoder[3];

CPipSetup::CPipSetup()
{
	frameBuffer = CFrameBuffer::getInstance();

	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoModes::mode_radio)
	{
		gx = &g_settings.pip_radio_x;
		gy = &g_settings.pip_radio_y;
		gw = &g_settings.pip_radio_width;
		gh = &g_settings.pip_radio_height;
	}
	else
	{
		gx = &g_settings.pip_x;
		gy = &g_settings.pip_y;
		gw = &g_settings.pip_width;
		gh = &g_settings.pip_height;
	}
	gp = &g_settings.pip_rotate_lastpos;

	x_coord = *gx;
	y_coord = *gy;
	width = *gw;
	height = *gh;
	pos = *gp;

	maxw = frameBuffer->getScreenWidth(true);
	maxh = frameBuffer->getScreenHeight(true);
	minw = maxw / 8;
	minh = minw * 1000 / 1825;
}

void CPipSetup::move(int x, int y, bool abs)
{
	int newx, newy;

	if (abs)
	{
		newx = x;
		newy = y;
	}
	else
	{
		newx = x_coord + x;
		newy = y_coord + y;
		if (newx < 0)
			newx = 0;
		else if ((newx + width) >= maxw)
			newx = maxw - width;
		if (newy < 0)
			newy = 0;
		else if ((newy + height) >= maxh)
			newy = maxh - height;
	}

	x_coord = newx;
	y_coord = newy;
	*gx = x_coord;
	*gy = y_coord;

	printf("CPipSetup::move: x %d y %d\n", x_coord, y_coord);
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->Pig(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height, maxw, maxh);
}

void CPipSetup::rotate(int cw)
{
	pos += cw;
	if (pos < PIP_UP_LEFT)
		pos = PIP_DOWN_LEFT;
	if (pos > PIP_DOWN_LEFT)
		pos = PIP_UP_LEFT;

	*gp = pos;

	printf("CPipSetup::rotate: x %d y %d\n", CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord));
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->Pig(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height, maxw, maxh);
}

// w and h is percent, if not absolute
void CPipSetup::resize(int w, int h, bool abs)
{
	int neww, newh;

	if (abs)
	{
		width = w;
		height = h;
	}
	else
	{
		neww = width + (maxw * w / 100);
		newh = height + (maxh * h / 100);

		if (neww > maxw)
			neww = maxw;
		else if (neww < minw)
			neww = minw;

		if (newh > maxh)
			newh = maxh;
		else if (newh < minh)
			newh = minh;
		if ((x_coord + neww) > maxw)
			return;
		if ((y_coord + newh) > maxh)
			return;
		width = neww;
		height = newh;
	}

	*gw = width;
	*gh = height;

	printf("CPipSetup::resize: w %d h %d \n", width, height);
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->Pig(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height, maxw, maxh);
}

int CPipSetup::exec(CMenuTarget *parent, const std::string &)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int oldx = x_coord;
	int oldy = y_coord;
	int oldw = width;
	int oldh = height;

	printf("CPipSetup::exec\n");
	int res = menu_return::RETURN_REPAINT;

	if (parent)
		parent->hide();

	paint();

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

#if !HAVE_CST_HARDWARE
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->ShowPig(1);
#endif
	bool loop = true;
	while (loop)
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if (msg <= CRCInput::RC_MaxRC)
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if ((msg == (neutrino_msg_t) g_settings.key_pip_setup) || (msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok))
		{
			loop = false;
			break;
		}
		else if (CNeutrinoApp::getInstance()->backKey(msg) || (msg == CRCInput::RC_spkr))
		{
			clear();
			move(oldx, oldy, true);
			resize(oldw, oldh, true);
			paint();
			if (CNeutrinoApp::getInstance()->backKey(msg))
				loop = false;
		}
		else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down))
		{
			clear();
			move(0, (msg == CRCInput::RC_up) ? -YMOVE : YMOVE);
			paint();
		}
		else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right))
		{
			clear();
			move((msg == CRCInput::RC_left) ? -XMOVE : XMOVE, 0);
			paint();
		}
		else if ((msg == CRCInput::RC_plus) || (msg == CRCInput::RC_minus))
		{
			clear();
			int percent = (msg == CRCInput::RC_plus) ? PERCENT : -PERCENT;
			resize(percent, percent);
			paint();
		}
		else if ((msg == (neutrino_msg_t) g_settings.key_pip_rotate_cw) || (msg == (neutrino_msg_t) g_settings.key_pip_rotate_ccw))
		{
			clear();
			int rotate_cw = (msg == (neutrino_msg_t) g_settings.key_pip_rotate_cw) ? 1 : -1;
			rotate(rotate_cw);
			paint();
		}
		else if (msg >  CRCInput::RC_MaxRC)
		{
			if (CNeutrinoApp::getInstance()->handleMsg(msg, data) & messages_return::cancel_all)
			{
				loop = false;
				res = menu_return::RETURN_EXIT_ALL;
			}
		}
	}
	hide();
	return res;
}

void CPipSetup::hide()
{
	frameBuffer->Clear();

#if 0 //!HAVE_CST_HARDWARE
	if (pipVideoDecoder[0] != NULL)
		pipVideoDecoder[0]->ShowPig(0);
#endif
}

void CPipSetup::clear()
{
	frameBuffer->paintBackgroundBoxRel(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height);
}

void CPipSetup::paint()
{
	if (!frameBuffer->getActive())
		return;

	char xpos[30];
	char ypos[30];
	char wpos[30];
	char hpos[30];
	char ppos[30];

	sprintf(xpos, "X: %d", x_coord);
	sprintf(ypos, "Y: %d", y_coord);
	sprintf(wpos, "W: %d", width);
	sprintf(hpos, "H: %d", height);
	sprintf(ppos, "P: %d", pos);

	int mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	int mwidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("W: 9999");

	int w = mwidth + 2 * OFFSET_INNER_MID;
	int h = mheight * 5;
	int x = (frameBuffer->getScreenWidth() - w) / 2;
	int y = (frameBuffer->getScreenHeight() - h) / 2;

	if (pipVideoDecoder[0] != NULL)
	{
		if (pipVideoDecoder[0]->getBlank())
			frameBuffer->paintBoxRel(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height, COL_MENUCONTENT_PLUS_0);
	}
	else
		frameBuffer->paintBoxRel(CNeutrinoApp::getInstance()->pip_recalc_pos_x(x_coord), CNeutrinoApp::getInstance()->pip_recalc_pos_y(y_coord), width, height, COL_MENUCONTENT_PLUS_0);

	frameBuffer->paintBoxRel(x, y, w, h, COL_MENUCONTENT_PLUS_0);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, y + mheight,     mwidth, xpos, COL_MENUCONTENT_TEXT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, y + mheight * 2, mwidth, ypos, COL_MENUCONTENT_TEXT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, y + mheight * 3, mwidth, wpos, COL_MENUCONTENT_TEXT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, y + mheight * 4, mwidth, hpos, COL_MENUCONTENT_TEXT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + OFFSET_INNER_MID, y + mheight * 5, mwidth, ppos, COL_MENUCONTENT_TEXT);
}

#endif //#if ENABLE_PIP

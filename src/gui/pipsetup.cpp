#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/color.h>
#include <gui/widget/messagebox.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <system/settings.h>

#include <global.h>
#include <neutrino.h>
#include <gui/pipsetup.h>
#include <video.h>

#define PERCENT 5
#define XMOVE 10
#define YMOVE 10

#ifdef ENABLE_PIP

extern cVideo *pipDecoder;

CPipSetup::CPipSetup()
{
	frameBuffer = CFrameBuffer::getInstance();
	x_coord = g_settings.pip_x;
	y_coord = g_settings.pip_y;
	width = g_settings.pip_width;
	height = g_settings.pip_height;

	maxw = frameBuffer->getScreenWidth(true);
	maxh = frameBuffer->getScreenHeight(true);
	minw = maxw/8;
	minh = minw*1000/1825;
}

void CPipSetup::move(int x, int y, bool abs)
{
	int newx, newy;

	if(abs) {
		newx = x;
		newy = y;
	} else {
		newx = x_coord + x;
		newy = y_coord + y;
		if(newx < 0) newx = 0;
		else if((newx + width) >= maxw) newx = maxw - width;
		if(newy < 0) newy = 0;
		else if((newy + height) >= maxh) newy = maxh - height;
	}
	x_coord = newx;
	y_coord = newy;
	g_settings.pip_x = x_coord;
	g_settings.pip_y = y_coord;

	printf("CPipSetup::move: x %d y %d\n", x_coord, y_coord);
	pipDecoder->Pig(x_coord, y_coord, width, height, maxw, maxh);
}

// w and h is percent, if not absolute
void CPipSetup::resize(int w, int h, bool abs)
{
	int neww, newh;

	if(abs) {
		width = w;
		height = h;
	} else {
		neww = width + (maxw*w/100);
		//newh = neww * 100 / 125;
		newh = height + (maxh*h/100);

		if(neww > maxw) neww = maxw;
		else if(neww < minw) neww = minw;

		if(newh > maxh) newh = maxh;
		else if(newh < minh) newh = minh;
		if( (x_coord + neww) > maxw)
			return;
		if( (y_coord + newh) > maxh)
			return;
		width = neww;
		height = newh;
	}
	g_settings.pip_width = width;
	g_settings.pip_height = height;

	printf("CPipSetup::resize: w %d h %d \n", width, height);
	pipDecoder->Pig(x_coord, y_coord, width, height, maxw, maxh);
}

int CPipSetup::exec(CMenuTarget* parent, const std::string &)
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

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

	bool loop=true;
	while (loop) {
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd, true);

		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_MENU] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_MENU]);

		if ((msg == (neutrino_msg_t) g_settings.key_pip_setup) ||
			(msg == CRCInput::RC_timeout) || (msg == CRCInput::RC_ok)) {
				loop = false;
				break;
		} else if ((msg == CRCInput::RC_home) || (msg == CRCInput::RC_spkr)) {
			clear();
			move(oldx, oldy, true);
			resize(oldw, oldh, true);
			paint();
			if (msg == CRCInput::RC_home)
				loop = false;
		} else if ((msg == CRCInput::RC_up) || (msg == CRCInput::RC_down)) {
			clear();
			move(0, (msg == CRCInput::RC_up) ? -YMOVE : YMOVE);
			paint();
		} else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right)) {
			clear();
			move((msg == CRCInput::RC_left) ? -XMOVE : XMOVE, 0);
			paint();
		} else if ((msg == CRCInput::RC_plus) || (msg == CRCInput::RC_minus)) {
			clear();
			int percent = (msg == CRCInput::RC_plus) ? PERCENT : -PERCENT;
			resize(percent, percent);
			paint();
		} else if (msg >  CRCInput::RC_MaxRC) {
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) {
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
}

void CPipSetup::clear()
{
	frameBuffer->paintBackgroundBoxRel(x_coord, y_coord, width, height);
}

void CPipSetup::paint()
{
	if (!frameBuffer->getActive())
		return;

	char xpos[30];
	char ypos[30];
	char wpos[30];
	char hpos[30];

	sprintf(xpos, "X: %d", x_coord );
	sprintf(ypos, "Y: %d", y_coord );
	sprintf(wpos, "W: %d", width );
	sprintf(hpos, "H: %d", height );

	int mheight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	int mwidth = 10 + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("W: 9999");
	int x = (frameBuffer->getScreenWidth() - mwidth)/2;
	int y = (frameBuffer->getScreenHeight() - mheight*4)/2;

	if (pipDecoder->getBlank())
		frameBuffer->paintBoxRel(x_coord, y_coord, width, height, COL_MENUCONTENT_PLUS_0);

	frameBuffer->paintBoxRel(x, y, mwidth, mheight*4, COL_MENUCONTENT_PLUS_0);

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+5, y+mheight,   mwidth, xpos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+5, y+mheight*2, mwidth, ypos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+5, y+mheight*3, mwidth, wpos, COL_MENUCONTENT);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+5, y+mheight*4, mwidth, hpos, COL_MENUCONTENT);
}

#endif //#ifdef ENABLE_PIP

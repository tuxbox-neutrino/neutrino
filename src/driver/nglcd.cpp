/*
	Neutrino graphlcd daemon thread

	(C) 2012-2014 by martii


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
#include <global.h>
#include <neutrino.h>
#include <algorithm>
#include <system/debug.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <hardware_caps.h>
#include <driver/nglcd.h>
#include <eitd/sectionsd.h>
#include <math.h>

static const char * kDefaultConfigFile = "/etc/graphlcd.conf";
static nGLCD *nglcd = NULL;

extern CPictureViewer * g_PicViewer;

nGLCD::nGLCD() {
	lcd = NULL;
	Channel = "Neutrino";
	Epg = std::string(g_info.hw_caps->boxvendor) + " " + std::string(g_info.hw_caps->boxname);

	sem_init(&sem, 0, 1);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);

	channelLocked = false;
	doRescan = false;
	doStandby = false;
	doStandbyTime = false;
	doShowVolume = false;
	doSuspend = false;
	doExit = false;
	doMirrorOSD = false;
	fontsize_channel = 0;
	fontsize_epg = 0;
	fontsize_time = 0;
	fontsize_time_standby = 0;
	fonts_initialized = false;
	doScrollChannel = false;
	doScrollEpg = false;
	percent_channel = 0;
	percent_time = 0;
	percent_time_standby = 0;
	percent_epg = 0;
	percent_bar = 0;
	percent_space = 0;
	percent_logo = 0;
	Scale = 0;
	bitmap = NULL;
	blitFlag = true;

	nglcd = this;

	if (!g_settings.glcd_enable)
		doSuspend = true;

	if (pthread_create (&thrGLCD, 0, nGLCD::Run, this) != 0 )
		fprintf(stderr, "ERROR: pthread_create(nGLCD::Init)\n");

	Update();
}

void nGLCD::Lock(void)
{
	if (nglcd)
		pthread_mutex_lock(&nglcd->mutex);
}

void nGLCD::Unlock(void)
{
	if (nglcd)
		pthread_mutex_unlock(&nglcd->mutex);
}

nGLCD::~nGLCD() {
	Suspend();
	nglcd = NULL;
	if (lcd) {
		lcd->DeInit();
		delete lcd;
	}
	sem_destroy(&sem);
	pthread_mutex_destroy(&mutex);
}

nGLCD *nGLCD::getInstance()
{
	if (!nglcd)
		nglcd = new nGLCD;
	return nglcd;
}

void nGLCD::LcdAnalogClock(int posx,int posy,int dia)
{
	int tm_,th_,mx_,my_,hx_,hy_;
	double pi_ = 3.1415926535897932384626433832795, mAngleInRad, mAngleSave, hAngleInRad;

	tm_ = tm->tm_min;
	th_ = tm->tm_hour;

	mAngleInRad = ((6 * tm_) * (2*pi_ / 360));
	mAngleSave = mAngleInRad;
	mAngleInRad -= pi_/2;

	mx_ = int((dia * 0.7 * cos(mAngleInRad)));
	my_ = int((dia * 0.7 * sin(mAngleInRad)));

	hAngleInRad = ((30 * th_)* (2*pi_ / 360));
	hAngleInRad += mAngleSave / 12;
	hAngleInRad -= pi_/2;
	hx_ = int((dia * 0.4 * cos(hAngleInRad)));
	hy_ = int((dia * 0.4 * sin(hAngleInRad)));

	std::string a_clock = "";

	a_clock = ICONSDIR "/a_clock.png";
	if (access(a_clock.c_str(), F_OK) != 0)
		a_clock = ICONSDIR "/a_clock.png";

	int lcd_a_clock_width = 0, lcd_a_clock_height = 0;
	g_PicViewer->getSize(a_clock.c_str(), &lcd_a_clock_width, &lcd_a_clock_height);
	if (lcd_a_clock_width && lcd_a_clock_height)
	{
		showImage(a_clock, (uint32_t) lcd_a_clock_width, (uint32_t) lcd_a_clock_height,
			0, 0, (uint32_t) nglcd->bitmap->Width(), (uint32_t) nglcd->bitmap->Height(), false, false);

		lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
		lcd->Refresh(true);
	}

	// hour
	bitmap->DrawLine(posx,posy-8,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-7,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-6,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-5,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-4,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-3,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-2,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-1,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx+1,posy,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+1,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+2,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+3,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+4,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+5,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+6,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+7,posx+hx_,posy+hy_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+8,posx+hx_,posy+hy_, g_settings.glcd_color_fg);

	// minute
	bitmap->DrawLine(posx,posy-6,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-5,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-4,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-3,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-2,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy-1,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx+1,posy,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+1,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+2,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+3,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+4,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+5,posx+mx_,posy+my_, g_settings.glcd_color_fg);
	bitmap->DrawLine(posx,posy+6,posx+mx_,posy+my_, g_settings.glcd_color_fg);
}

void nGLCD::Exec() {
	if (!lcd)
		return;

	bitmap->Clear(g_settings.glcd_color_bg);

	if (Channel == "Neutrino") {
		if (g_settings.glcd_show_logo) {
			int start_width = 0, start_height = 0;
			g_PicViewer->getSize(DATADIR "/neutrino/icons/start.jpg", &start_width, &start_height);
			if (start_width && start_height) {
				showImage(DATADIR "/neutrino/icons/start.jpg", (uint32_t) start_width, (uint32_t) start_height,
					0, 0, (uint32_t) nglcd->bitmap->Width(), (uint32_t) nglcd->bitmap->Height(), false, true);

				GLCD::cFont font_tmp;

				int fw = font_epg.Width(Epg);
				font_tmp.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_epg * (bitmap->Width() - 4) / fw);
				fw = font_tmp.Width(Epg);

				bitmap->DrawText(std::max(2,(bitmap->Width() - 4 - fw)/2),
					10 * bitmap->Height()/100, bitmap->Width() - 4, Epg,
					&font_tmp, g_settings.glcd_color_fg, GLCD::cColor::Transparent);

				lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
				lcd->Refresh(true);
			}
		} else {
			nglcd->bitmap->Clear(g_settings.glcd_color_bg);
			nglcd->lcd->Refresh(true);
		}
		return;
	}

	if (doStandbyTime) {
		if (percent_time_standby) {

			std::string Time;
			if (g_settings.glcd_time_in_standby == 3)
				LcdAnalogClock(bitmap->Width()/2, bitmap->Height()/2, 200);
			else if (g_settings.glcd_time_in_standby == 2)
				Time = strftime("%H:%M:%S", tm);
			else
				Time = strftime("%H:%M", tm);

			bitmap->DrawText(std::max(2,(bitmap->Width() - 4 - font_time_standby.Width(Time))/2),
				(bitmap->Height() - font_time_standby.Height(Time))/2, bitmap->Width() - 1, Time,
				&font_time_standby, g_settings.glcd_color_fg, GLCD::cColor::Transparent);
			lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
			lcd->Refresh(false);
		}
		return;
	}

	if (CNeutrinoApp::getInstance()->recordingstatus) {
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K
		for (int bx = 0; bx < 6; bx++) {
#else
		for (int bx = 0; bx < 3; bx++) {
#endif
			bitmap->DrawRectangle(bx, bx, bitmap->Width() - bx + 1, bitmap->Height() - bx + 1, GLCD::cColor::Red, false);
		}
	} else
	if (CNeutrinoApp::getInstance()->isMuted()) {
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K
		for (int bx = 0; bx < 6; bx++) {
#else
		for (int bx = 0; bx < 3; bx++) {
#endif
			bitmap->DrawRectangle(bx, bx, bitmap->Width() - bx + 1, bitmap->Height() - bx + 1, GLCD::cColor::Blue, false);
		}
	}

	int off = percent_space;

	if (g_settings.glcd_show_logo && percent_logo &&
		showImage(channel_id, Channel, 0, off * bitmap->Height()/100, bitmap->Width() - 4, percent_logo * bitmap->Height()/100, true)) {
		off += percent_logo;
		off += percent_space;
		doScrollChannel = false;
		scrollChannelSkip = 0;
	} else if (percent_channel) {
		int logo_offset = 0;
		if (g_settings.glcd_show_logo && percent_channel < percent_logo) {
			int o = logo_offset = percent_logo - percent_channel;
			o >>= 1;
			off += o;
			logo_offset -= o;
		}
		if (ChannelWidth) {
			if (scrollChannelForward) {
				if (ChannelWidth - scrollChannelSkip < bitmap->Width() - 4)
					scrollChannelForward = false;
			} else if (scrollChannelSkip <= 0) {
				scrollChannelSkip = 0;
				doScrollChannel = false;
			}

			bitmap->DrawText(std::max(2,(bitmap->Width() - 4 - ChannelWidth)/2) + scrollChannelOffset,
				off * bitmap->Height()/100, bitmap->Width() - 4, Channel,
				&font_channel, g_settings.glcd_color_fg, GLCD::cColor::Transparent, true, scrollChannelSkip);

			if (scrollChannelOffset > 0)
				scrollChannelOffset -= g_settings.glcd_scroll_speed;

			if (scrollChannelOffset < 0)
				scrollChannelOffset = 0;

			if (scrollChannelOffset == 0) {
				if (scrollChannelForward)
					scrollChannelSkip += g_settings.glcd_scroll_speed;
				else
					scrollChannelSkip -= g_settings.glcd_scroll_speed;
			}
		}
		off += percent_channel;
		off += logo_offset;
		off += percent_space;
	} else
		off = 0;

	if (percent_epg) {
		off += percent_space;
		if (EpgWidth) {
			if (scrollEpgForward) {
				if (EpgWidth - scrollEpgSkip < bitmap->Width() - 4)
					scrollEpgForward = false;
			} else if (scrollEpgSkip <= 0) {
				scrollEpgSkip = 0;
				doScrollEpg = false;
			}

			bitmap->DrawText(std::max(2,(bitmap->Width() - 4 - EpgWidth)/2) + scrollEpgOffset,
				off * bitmap->Height()/100, bitmap->Width() - 4, Epg,
				&font_epg, g_settings.glcd_color_fg, GLCD::cColor::Transparent, true, scrollEpgSkip);

			if (scrollEpgOffset > 0)
				scrollEpgOffset -= g_settings.glcd_scroll_speed;

			if (scrollEpgOffset < 0)
				scrollEpgOffset = 0;

			if (scrollEpgOffset == 0) {
				if (scrollEpgForward)
					scrollEpgSkip += g_settings.glcd_scroll_speed;
				else
					scrollEpgSkip -= g_settings.glcd_scroll_speed;
			}
		}
		off += percent_epg;
		off += percent_space;
	}

	if (percent_bar) {
		off += percent_space;
		int bar_top = off * bitmap->Height()/100;
		off += percent_bar;
		int bar_bottom = off * bitmap->Height()/100;
		bitmap->DrawHLine(0, bar_top, bitmap->Width(), g_settings.glcd_color_fg);
		bitmap->DrawHLine(0, bar_bottom, bitmap->Width(), g_settings.glcd_color_fg);
		if (Scale)
			bitmap->DrawRectangle(0, bar_top + 1, Scale * (bitmap->Width() - 1)/100,
				bar_bottom - 1, g_settings.glcd_color_bar, true);
		off += percent_space;
	}

	if (percent_time) {
		off += percent_space;
		std::string Time = strftime("%H:%M", tm);
		bitmap->DrawText(std::max(2,(bitmap->Width() - 4 - font_time.Width(Time))/2),
			off * bitmap->Height()/100, bitmap->Width() - 1, Time,
			&font_time, g_settings.glcd_color_fg, GLCD::cColor::Transparent);
	}

	lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
	lcd->Refresh(false);
}

void nGLCD::updateFonts() {
	int percent;
	percent = std::max(g_settings.glcd_percent_channel, g_settings.glcd_show_logo ? g_settings.glcd_percent_logo : 0)
		+ g_settings.glcd_percent_epg + g_settings.glcd_percent_bar + g_settings.glcd_percent_time;

	int div = 0;

	if (percent_channel || percent_logo)
		div += 2;
	if (percent_epg)
		div += 2;
	if (percent_bar)
		div += 2;
	if (percent_time)
		div += 2;

	percent += div;

	if (percent < 100)
		percent = 100;

	percent_logo = g_settings.glcd_show_logo ? g_settings.glcd_percent_logo * 100 / percent : 0;
	percent_channel = g_settings.glcd_percent_channel * 100 / percent;
	percent_epg = g_settings.glcd_percent_epg * 100 / percent;
	percent_bar = g_settings.glcd_percent_bar * 100 / percent;
	percent_time = g_settings.glcd_percent_time * 100 / percent;
	percent_time_standby = std::min(g_settings.glcd_percent_time_standby, 100);

	percent_space = (100 - std::max(percent_logo, percent_channel) - percent_time - percent_epg - percent_bar) / div;

	// calculate height
	int fontsize_channel_new = percent_channel * nglcd->lcd->Height() / 100;
	int fontsize_epg_new = percent_epg * nglcd->lcd->Height() / 100;
	int fontsize_time_new = percent_time * nglcd->lcd->Height() / 100;
	int fontsize_time_standby_new = percent_time_standby * nglcd->lcd->Height() / 100;

	if (!fonts_initialized || (fontsize_channel_new != fontsize_channel)) {
		fontsize_channel = fontsize_channel_new;
		if (!font_channel.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_channel)) {
			g_settings.glcd_font = FONTDIR "/neutrino.ttf";
			font_channel.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_channel);
		}
	}
	if (!fonts_initialized || (fontsize_epg_new != fontsize_epg)) {
		fontsize_epg = fontsize_epg_new;
		if (!font_epg.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_epg)) {
			g_settings.glcd_font = FONTDIR "/neutrino.ttf";
			font_epg.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_epg);
		}
	}
	if (!fonts_initialized || (fontsize_time_new != fontsize_time)) {
		fontsize_time = fontsize_time_new;
		if (!font_time.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_time)) {
			g_settings.glcd_font = FONTDIR "/neutrino.ttf";
			font_time.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_time);
		}
	}
	if (!fonts_initialized || (fontsize_time_standby_new != fontsize_time_standby)) {
		fontsize_time_standby = fontsize_time_standby_new;
		if (!font_time_standby.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_time_standby)) {
			g_settings.glcd_font = FONTDIR "/neutrino.ttf";
			font_time_standby.LoadFT2(g_settings.glcd_font, "UTF-8", fontsize_time_standby);
		}
	}

	fonts_initialized = true;
}

bool nGLCD::getBoundingBox(uint32_t *buffer, int width, int height, int &bb_x, int &bb_y, int &bb_w, int &bb_h)
{
	if (!width || !height) {
		bb_x = bb_y = bb_w = bb_h = 0;
		return false;
	}

	int y_min = height;
	uint32_t *b = buffer;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++, b++)
			if (*b) {
				y_min = y;
				goto out1;
			}
	out1:

	int y_max = y_min;
	b = buffer + height * width - 1;
	for (int y = height - 1; y_min < y; y--)
		for (int x = 0; x < width; x++, b--)
			if (*b) { 
				y_max = y;
				goto out2;
			}
	out2:

	int x_min = width;
	for (int x = 0; x < width; x++) {
		b = buffer + x + y_min * width;
		for (int y = y_min; y < y_max; y++, b += width)
			if (*b) {
				x_min = x;
				goto out3;
			}
	}
	out3:

	int x_max = x_min;
	for (int x = width - 1; x_min < x; x--) {
		b = buffer + x + y_min * width;
		for (int y = y_min; y < y_max; y++, b += width)
			if (*b) {
				x_max = x;
				goto out4;
			}
	}
	out4:

	bb_x = x_min;
	bb_y = y_min;
	bb_w = 1 + x_max - x_min;
	bb_h = 1 + y_max - y_min;

	if (bb_x < 0)
		bb_x = 0;
	if (bb_y < 0)
		bb_y = 0;

	return true;
}

void* nGLCD::Run(void *arg)
{
	nGLCD *me = (nGLCD *) arg;
	me->Run();
	pthread_exit(NULL);
}

void nGLCD::Run(void)
{
	set_threadname("nGLCD::Run");

	if (GLCD::Config.Load(kDefaultConfigFile) == false) {
		fprintf(stderr, "Error loading config file!\n");
		return;
	}
	if ((GLCD::Config.driverConfigs.size() < 1)) {
		fprintf(stderr, "No driver config found!\n");
		return;
	}

	struct timespec ts;

	CSectionsdClient::CurrentNextInfo info_CurrentNext;
	channel_id = -1;
	info_CurrentNext.current_zeit.startzeit = 0;
	info_CurrentNext.current_zeit.dauer = 0;
	info_CurrentNext.flags = 0;

	fonts_initialized = false;
	bool broken = false;

	do {
		if (broken) {
#ifdef GLCD_DEBUG
			fprintf(stderr, "No graphlcd display found ... sleeping for 30 seconds\n");
#endif
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 30;
			sem_timedwait(&sem, &ts);
			broken = false;
			if (doExit)
				break;
			if (!g_settings.glcd_enable)
				continue;
		} else
				while ((doSuspend || doStandby || !g_settings.glcd_enable) && !doExit)
					sem_wait(&sem);

		if (doExit)
			break;

		int warmUp = 10;
		lcd = GLCD::CreateDriver(GLCD::Config.driverConfigs[0].id, &GLCD::Config.driverConfigs[0]);
		if (!lcd) {
#ifdef GLCD_DEBUG
			fprintf(stderr, "CreateDriver failed.\n");
#endif
			broken = true;
			continue;
		}
#ifdef GLCD_DEBUG
		fprintf(stderr, "CreateDriver succeeded.\n");
#endif
		if (lcd->Init()) {
			delete lcd;
			lcd = NULL;
#ifdef GLCD_DEBUG
			fprintf(stderr, "LCD init failed.\n");
#endif
			broken = true;
			continue;
		}
#ifdef GLCD_DEBUG
		fprintf(stderr, "LCD init succeeded.\n");
#endif
		lcd->SetBrightness(0);

		if (!bitmap)
			bitmap = new GLCD::cBitmap(lcd->Width(), lcd->Height(), g_settings.glcd_color_bg);

		UpdateBrightness();
		Update();

		doMirrorOSD = false;

		while ((!doSuspend && !doStandby) && !doExit && g_settings.glcd_enable) {
			if (doMirrorOSD && !doStandbyTime) {
				if (blitFlag) {
					blitFlag = false;
					bitmap->Clear(GLCD::cColor::Black);
					ts.tv_sec = 0; // don't wait
					static CFrameBuffer* fb = CFrameBuffer::getInstance();
#if !defined BOXMODEL_VUSOLO4K && !defined BOXMODEL_VUDUO4K && !defined BOXMODEL_VUULTIMO4K
					static int fb_width = fb->getScreenWidth(true);
#endif
					static int fb_height = fb->getScreenHeight(true);
					static uint32_t *fbp = fb->getFrameBufferPointer();
					int lcd_width = bitmap->Width();
					int lcd_height = bitmap->Height();
#if BOXMODEL_VUSOLO4K || BOXMODEL_VUDUO4K || BOXMODEL_VUULTIMO4K
					unsigned int fb_stride = fb->getStride()/4;
					if (!showImage(fbp, fb_stride, fb_height, 0, 0, lcd_width, lcd_height, false)) {
#else
					if (!showImage(fbp, fb_width, fb_height, 0, 0, lcd_width, lcd_height, false)) {
#endif
						usleep(500000);
					} else {
						lcd->SetScreen(bitmap->Data(), lcd_width, lcd_height);
						lcd->Refresh(false);
					}
				} else
					usleep(100000);
				continue;
			}
			if (g_settings.glcd_mirror_video && !doStandbyTime) {
				char ws[10];
				snprintf(ws, sizeof(ws), "%d", bitmap->Width());
				const char *bmpShot = "/tmp/nglcd-video.bmp";
				my_system(4, "/bin/grab", "-vr", ws, bmpShot);
				int bw = 0, bh = 0;
				g_PicViewer->getSize(bmpShot, &bw, &bh);
				if (bw > 0 && bh > 0) {
					int lcd_width = bitmap->Width();
					int lcd_height = bitmap->Height();
					if (!showImage(bmpShot, (uint32_t) bw, (uint32_t) bh, 0, 0, (uint32_t) lcd_width, lcd_height, false, true))
						usleep(1000000);
					else {
						lcd->SetScreen(bitmap->Data(), lcd_width, lcd_height);
							lcd->Refresh(false);
					}
				}
				else
						usleep(1000000);
				continue;
			}

			clock_gettime(CLOCK_REALTIME, &ts);
			tm = localtime(&ts.tv_sec);
			updateFonts();
			Exec();
			clock_gettime(CLOCK_REALTIME, &ts);
			tm = localtime(&ts.tv_sec);
			if (warmUp > 0) {
				ts.tv_sec += 1;
				warmUp--;
			} else {
				ts.tv_sec += 60 - tm->tm_sec;
				ts.tv_nsec = 0;
			}

			if (!doScrollChannel && !doScrollEpg)
				sem_timedwait(&sem, &ts);

			while(!sem_trywait(&sem));

			if(doRescan || doSuspend || doStandby || doExit)
				break;

			if (doShowVolume) {
				Epg = "";
				if (Channel.compare(g_Locale->getText(LOCALE_GLCD_VOLUME))) {
					Channel = g_Locale->getText(LOCALE_GLCD_VOLUME);
					ChannelWidth = font_channel.Width(Channel);
					doScrollChannel = ChannelWidth > bitmap->Width() - 4;
					scrollChannelSkip = 0;
					scrollChannelForward = true;
					if (doScrollChannel) {
						scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						ChannelWidth += scrollChannelOffset;
					} else
						scrollChannelOffset = 0;
				}
				EpgWidth = 0;
				scrollEpgSkip = 0;
				scrollEpgForward = true;
				Scale = g_settings.current_volume;
				channel_id = -1;
			} else if (channelLocked) {
				Lock();
				if (Epg.compare(stagingEpg)) {
					Epg = stagingEpg;
					EpgWidth = font_epg.Width(Epg);
					doScrollEpg = EpgWidth > bitmap->Width() - 4;
					scrollEpgSkip = 0;
					scrollEpgForward = true;
					if (doScrollEpg) {
						scrollEpgOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						EpgWidth += scrollEpgOffset;
					} else
						scrollChannelOffset = 0;
				}
				if (Channel.compare(stagingChannel)) {
					Channel = stagingChannel;
					ChannelWidth = font_channel.Width(Channel);
					doScrollChannel = ChannelWidth > bitmap->Width() - 4;
					scrollChannelSkip = 0;
					scrollChannelForward = true;
					if (doScrollChannel) {
						scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						ChannelWidth += scrollChannelOffset;
					} else
						scrollChannelOffset = 0;
				}
				channel_id = -1;
				Unlock();
			} else {
				CChannelList *channelList = CNeutrinoApp::getInstance ()->channelList;
				if (!channelList)
					continue;
				t_channel_id new_channel_id = channelList->getActiveChannel_ChannelID();
				if (!new_channel_id)
					continue;

				if ((new_channel_id != channel_id)) {
					Channel = channelList->getActiveChannelName ();
					ChannelWidth = font_channel.Width(Channel);
					Epg = "";
					EpgWidth = 0;
					Scale = 0;
					doScrollEpg = false;
					doScrollChannel = ChannelWidth > bitmap->Width() - 4;
					scrollChannelForward = true;
					scrollChannelSkip = 0;
					if (doScrollChannel) {
						scrollChannelOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						ChannelWidth += scrollChannelOffset;
					} else
						scrollChannelOffset = 0;
					warmUp = 10;
					info_CurrentNext.current_name = "";
					info_CurrentNext.current_zeit.dauer = 0;
				}

				CEitManager::getInstance()->getCurrentNextServiceKey(channel_id & 0xFFFFFFFFFFFFULL, info_CurrentNext);
				channel_id = new_channel_id;

				if (info_CurrentNext.current_name.compare(Epg)) {
					Epg = info_CurrentNext.current_name;
					EpgWidth = font_epg.Width(Epg);
					doScrollEpg = EpgWidth > bitmap->Width() - 4;
					scrollEpgForward = true;
					scrollEpgSkip = 0;
					if (doScrollEpg) {
						scrollEpgOffset = bitmap->Width()/g_settings.glcd_scroll_speed;
						EpgWidth += scrollEpgOffset;
					} else
						scrollEpgOffset = 0;
				}
				if (info_CurrentNext.current_zeit.dauer > 0)
					Scale = (ts.tv_sec - info_CurrentNext.current_zeit.startzeit) * 100 / info_CurrentNext.current_zeit.dauer;
				if (Scale > 100)
					Scale = 100;
				else if (Scale < 0)
					Scale = 0;
			}
		}

		if(!g_settings.glcd_enable || doSuspend || doStandby || doExit) {
			// for restart, don't blacken screen
			bitmap->Clear(GLCD::cColor::Black);
			lcd->SetBrightness(0);
			lcd->SetScreen(bitmap->Data(), bitmap->Width(), bitmap->Height());
			lcd->Refresh(false);
		}
		if (doRescan) {
			doRescan = false;
			Update();
		}
		lcd->DeInit();
		delete lcd;
		lcd = NULL;
	} while(!doExit);
}

void nGLCD::Update() {
	if (nglcd)
		sem_post(&nglcd->sem);
}

void nGLCD::StandbyMode(bool b) {
	if (nglcd) {
		if (g_settings.glcd_time_in_standby) {
			nglcd->doStandbyTime = b;
			nglcd->doStandby = false;
		} else {
			nglcd->doStandbyTime = false;
			nglcd->doStandby = b;
		}
		if (b) {
			nglcd->doScrollChannel = false;
			nglcd->doScrollEpg = false;
		} else {
			nglcd->doScrollChannel = true;
			nglcd->doScrollEpg = true;
		}
		nglcd->doMirrorOSD = false;
		nglcd->UpdateBrightness();
		nglcd->Update();
	}
}

void nGLCD::ShowVolume(bool b) {
	if (nglcd) {
		nglcd->doShowVolume = b;
		nglcd->Update();
	}
}

void nGLCD::MirrorOSD(bool b) {
	if (nglcd) {
		nglcd->doMirrorOSD = b;
		nglcd->Update();
	}
}

void nGLCD::Exit() {
	if (nglcd) {
		nglcd->doMirrorOSD = false;
		nglcd->doSuspend = false;
		nglcd->doExit = true;
		nglcd->Update();
		void *res;
		pthread_join(nglcd->thrGLCD, &res);
		delete nglcd;
		nglcd = NULL;
	}
}

void nGLCD::Rescan() {
	doRescan = true;
	Update();
}

void nGLCD::Suspend() {
	if (nglcd) {
		nglcd->doSuspend = true;
		nglcd->Update();
	}
}

void nGLCD::Resume() {
	if (nglcd) {
		nglcd->doSuspend = false;
		nglcd->channelLocked = false;
		nglcd->Update();
	}
}

void nGLCD::lockChannel(std::string c, std::string e, int s)
{
	if(nglcd) {
		nglcd->Lock();
		nglcd->channelLocked = true;
		nglcd->stagingChannel = c;
		nglcd->stagingEpg = e;
		nglcd->Scale = s;
		nglcd->Unlock();
		nglcd->Update();
	}
}

void nGLCD::unlockChannel(void)
{
	if(nglcd) {
		nglcd->channelLocked = false;
		nglcd->Update();
	}
}

bool nGLCD::showImage(fb_pixel_t *s, uint32_t sw, uint32_t sh, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	int bb_x, bb_y, bb_w, bb_h;

	if (nglcd->getBoundingBox(s, sw, sh, bb_x, bb_y, bb_w, bb_h) && bb_w && bb_h) {
		if (!maximize) {
			if (bb_h * dw > bb_w * dh) {
				uint32_t dw_new = dh * bb_w / bb_h;
				dx += (dw - dw_new) >> 1;
				dw = dw_new;
			} else {
				uint32_t dh_new = dw * bb_h / bb_w;
				dy += (dh - dh_new) >> 1;
				dh = dh_new;
			}
		}
		for (u_int y = 0; y < dh; y++) {
			for (u_int x = 0; x < dw; x++) {
				uint32_t pix = *(s + (y * bb_h / dh + bb_y) * sw + x * bb_w / dw + bb_x);
				if (!transp || pix)
					nglcd->bitmap->DrawPixel(x + dx, y + dy, pix);
			}
		}
		return true;
	}
	return false;
}

bool nGLCD::showImage(const std::string & filename, uint32_t sw, uint32_t sh, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	bool res = false;
	if (!dw || !dh)
		return res;
	fb_pixel_t *s = g_PicViewer->getImage(filename, sw, sh);
	if (s && sw && sh)
		res = showImage(s, sw, sh, dx, dy, dw, dh, transp, maximize);
	if (s)
		free(s);
	return res;
}

bool nGLCD::showImage(uint64_t cid, std::string cname, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh, bool transp, bool maximize)
{
	std::string logo;
	int sw, sh;

	if (g_PicViewer->GetLogoName(cid, cname, logo, &sw, &sh)) {
		return showImage(logo, (uint32_t) sw, (uint32_t) sh, dx, dy, dw, dh, transp, maximize);
	}
	return false;
}

void nGLCD::UpdateBrightness()
{
	if (nglcd && nglcd->lcd)
		nglcd->lcd->SetBrightness((unsigned int) (nglcd->doStandbyTime ? g_settings.glcd_brightness_standby : g_settings.glcd_brightness));
}

void nGLCD::SetBrightness(unsigned int b)
{
	if (nglcd)
		nglcd->SetBrightness(b);
}

void nGLCD::Blit()
{
	if (nglcd)
		nglcd->blitFlag = true;
}

int nGLCD::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t /* data */)
{
	if (msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) {
		Update();
		return messages_return::handled;
	}

	return messages_return::unhandled;
}


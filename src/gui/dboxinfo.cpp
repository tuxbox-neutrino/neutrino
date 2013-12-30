/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


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
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include <gui/dboxinfo.h>
#include <gui/components/cc.h>

#include <global.h>
#include <neutrino.h>

#include <driver/abstime.h>
#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>
#include <driver/fade.h>

#include <zapit/femanager.h>

#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <system/sysload.h>
#include <system/helpers.h>
#include <map>
#include <iostream>
#include <fstream>

CDBoxInfoWidget::CDBoxInfoWidget()
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	mheight += mheight & 1;
	width = 0;
	height = 0;
	x = 0;
	y = 0;
}

int CDBoxInfoWidget::exec(CMenuTarget* parent, const std::string &)
{
	if (parent)
	{
		parent->hide();
	}
	COSDFader fader(g_settings.menu_Content_alpha);
	fader.StartFadeIn();

	paint();
	frameBuffer->blit();

	//int res = g_RCInput->messageLoop();
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	bool doLoop = true;

	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_MENU];

	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd( timeout == 0 ? 0xFFFF : timeout);
	uint32_t updateTimer = g_RCInput->addTimer(5*1000*1000, false);

	while (doLoop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetTimer())) {
			if(fader.Fade())
				doLoop = false;
		}
		else if((msg == NeutrinoMessages::EVT_TIMER) && (data == updateTimer)) {
			paint();
		}
		else if ( ( msg == CRCInput::RC_timeout ) ||
				( msg == CRCInput::RC_home ) ||
				( msg == CRCInput::RC_ok ) ) {
			if(fader.StartFadeOut()) {
				timeoutEnd = CRCInput::calcTimeoutEnd( 1 );
				msg = 0;
			} else
				doLoop = false;
		}
		else if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			doLoop = false;
		}
		else if((msg == CRCInput::RC_sat) || (msg == CRCInput::RC_favorites)) {
			g_RCInput->postMsg (msg, 0);
			res = menu_return::RETURN_EXIT_ALL;
			doLoop = false;
		}
		else
		{
			int mr = CNeutrinoApp::getInstance()->handleMsg( msg, data );

			if ( mr & messages_return::cancel_all )
			{
				res = menu_return::RETURN_EXIT_ALL;
				doLoop = false;
			}
			else if ( mr & messages_return::unhandled )
			{
				if ((msg <= CRCInput::RC_MaxRC) &&
						(data == 0))                     /* <- button pressed */
				{
					timeoutEnd = CRCInput::calcTimeoutEnd( timeout );
				}
			}
		}
		frameBuffer->blit();
	}

	hide();
	fader.Stop();
	g_RCInput->killTimer(updateTimer);
	return res;
}

void CDBoxInfoWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
	frameBuffer->blit();
}

static std::string bytes2string(uint64_t bytes, bool binary = true);

static std::string bytes2string(uint64_t bytes, bool binary)
{
	uint64_t b = bytes;
	uint64_t base = binary ? 1024 : 1000;
	uint64_t factor = 1;
	const char *unit = binary ? "\0KMGT" : "\0kMGT";

	while (b > base) {
		b /= base;
		factor *= base;
		if (!*(unit + 1))
			break;
		unit++;
	}

	char result[80];

	if (b < base)
		snprintf(result, sizeof(result), "%d%s%02d ", (int)b, g_Locale->getText(LOCALE_UNIT_DECIMAL),
			(int)((bytes - b * factor) * 100 / factor));
	else // no need for fractions for larger numbers
		snprintf(result, sizeof(result), "%d ", (int)bytes);

	std::string res(result);
	if (*unit) {
		res.append(1, *unit);
		if (binary)
			res.append(1, 'i');
	}
	res.append(1, 'B');
	return res;
}

void CDBoxInfoWidget::paint()
{
	int fontWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	int sizeWidth = fontWidth * 11;//9999.99 MiB
	int percWidth = fontWidth * 4 ;//100%
	int nameWidth = fontWidth * 17;//WWWwwwwwww
	height = hheight;
	height += mheight/2;	// space
	int cpuload_y0 = height;
	height += mheight;	// time
	height += mheight;	// uptime
	height += mheight;	// load
	int cpuload_y1 = height;
	height += mheight/2;	// space

	int frontend_count = CFEManager::getInstance()->getFrontendCount();
	if (frontend_count) {
		height += mheight * frontend_count;
		height += mheight/2;
	}

	int icon_w = 0, icon_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icon_w, &icon_h);

#define DBINFO_TOTAL 0
#define DBINFO_USED 1
#define DBINFO_FREE 2
	int memstat[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } }; // total, used, free
#define DBINFO_RAM 0
#define DBINFO_SWAP 1
	const char *memtype[2] = { "RAM", "Swap" };
	FILE *procmeminfo = fopen("/proc/meminfo", "r");
	if (procmeminfo) {
		char buf[80], a[80];
		int v;
		while (fgets(buf, sizeof(buf), procmeminfo))
			if (2 == sscanf(buf, "%[^:]: %d", a, &v)) {
				if (!strcasecmp(a, "MemTotal"))
					memstat[DBINFO_RAM][DBINFO_TOTAL] += v;
				else if (!strcasecmp(a, "MemFree"))
					memstat[DBINFO_RAM][DBINFO_FREE] += v;
				else if (!strcasecmp(a, "Inactive"))
					memstat[DBINFO_RAM][DBINFO_FREE] += v;
				else if (!strcasecmp(a, "SwapTotal"))
					memstat[DBINFO_SWAP][DBINFO_TOTAL] = v;
				else if (!strcasecmp(a, "SwapFree"))
					memstat[DBINFO_SWAP][DBINFO_FREE] += v;
			}
		fclose(procmeminfo);
	}
	bool have_swap = memstat[DBINFO_SWAP][DBINFO_TOTAL];
	height += mheight;		// header
	height += mheight;		// ram
	height += have_swap * mheight;	// swap
	height += mheight/2;		// space

	std::ifstream in;

	std::map<std::string,bool> mounts;
	in.open("/proc/mounts");
	if (in.is_open()) {
		struct stat rec_st;
		struct statfs s;
		if (stat(g_settings.network_nfs_recordingdir.c_str(), &rec_st)
		|| (!::statfs(g_settings.network_nfs_recordingdir.c_str(), &s) && ((s.f_type == 0x72b6) || (s.f_type == 0x5941ff53))))
			memset(&rec_st, 0, sizeof(rec_st));

		std::map<dev_t,std::string> seen;
		std::string line;

		while (getline(in, line)) {
			if ((line.at(0) != '/') && (line.find("rootfs") != 0))
				continue;
			size_t firstspace = line.find_first_of(' ');
			if (firstspace != string::npos) {
				firstspace++;
				size_t nextspace = line.find_first_of(' ', firstspace);
				if (nextspace == string::npos || line.find("nodev", nextspace + 1) != string::npos)
					continue;
				std::string mountpoint = line.substr(firstspace, nextspace - firstspace);
				struct stat st;
				if (stat(mountpoint.c_str(), &st) || (seen.find(st.st_dev) != seen.end()))
					continue;
				seen[st.st_dev] = mountpoint;
				bool is_rec = (st.st_dev == rec_st.st_dev);
				mounts[mountpoint] = is_rec;
				int icon_space = is_rec ? 10 + icon_w : 0;
				nameWidth = std::max(nameWidth, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(mountpoint, true) + icon_space + 10);
			}
		}
		in.close();
	}

	height += mheight;			// header
	height += mounts.size() * mheight;	// file systems
	height += mheight/2;			// space

	int offsetw = nameWidth+ (sizeWidth+10)*3 +10+percWidth+10;
	width = offsetw + 10 + 120;

	int diff = frameBuffer->getScreenWidth() - width;
	if (diff < 0) {
		width -= diff;
		offsetw -= diff;
		nameWidth -= diff;
	}
	height = h_max(height, 0);
	x = getScreenStartX(width);
	y = getScreenStartY(height);

	// fprintf(stderr, "CDBoxInfoWidget::CDBoxInfoWidget() x = %d, y = %d, width = %d height = %d\n", x, y, width, height);

	int ypos=y;

	//paint head
	std::string title(g_Locale->getText(LOCALE_EXTRA_DBOXINFO));
	std::map<std::string,std::string> cpuinfo;
	in.open("/proc/cpuinfo");
	if (in.is_open()) {
		std::string line;
		while (getline(in, line)) {
			size_t colon = line.find_first_of(':');
			if (colon != string::npos && colon > 1) {
				std::string key = line.substr(0, colon - 1);
				std::string val = line.substr(colon + 1);
				cpuinfo[trim(key)] = trim(val);
			}
		}
		in.close();
	}
	if (!cpuinfo["Hardware"].empty()) {
		title += ": ";
		title += cpuinfo["Hardware"];
	} else if (!cpuinfo["machine"].empty()) {
		title += ": ";
		title + cpuinfo["machine"];
	}

	CComponentsHeader header(x, ypos, width, hheight, title, NEUTRINO_ICON_SHELL);
	header.paint(CC_SAVE_SCREEN_NO);

	//paint body
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	ypos += hheight + mheight/2;

	cSysLoad *sysload = cSysLoad::getInstance();
	int data_last = sysload->data_last;

	char ubuf[80];
	time_t now = time(NULL);
	strftime(ubuf, sizeof(ubuf), "Time: %F %H:%M:%S%z", localtime(&now));
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, ubuf, COL_MENUCONTENT_TEXT);
	ypos += mheight;

	struct sysinfo info;
	sysinfo(&info);
	now -= info.uptime;
	strftime(ubuf, sizeof(ubuf), "Boot: %F %H:%M:%S%z", localtime(&now));
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, ubuf, COL_MENUCONTENT_TEXT);
	ypos += mheight;

	if (data_last > -1) {
		snprintf(ubuf, sizeof(ubuf), "Load: %d%s%d%%", data_last/10, g_Locale->getText(LOCALE_UNIT_DECIMAL), data_last%10);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, ubuf, COL_MENUCONTENT_TEXT);
	}
	ypos += mheight;

	int pbw = width - offsetw - 10;
	if (pbw > 8) /* smaller progressbar is not useful ;) */
	{
		unsigned int h = cpuload_y1 - cpuload_y0;
		cpuload_y0 += y;
		cpuload_y1 += y;
		frameBuffer->paintBoxRel(x + offsetw, cpuload_y0, pbw, h, COL_MENUCONTENT_PLUS_2);

		int off = std::max(0, (int)sysload->data_avail - pbw);
		for (unsigned int i = 0; i < sysload->data_avail - off; i++) {
			if (sysload->data[i + off] > -1)
				frameBuffer->paintVLine(x+offsetw + i, cpuload_y1 - sysload->data[i + off] * h / 1000, cpuload_y1, COL_MENUCONTENT_PLUS_7);
		}
	}

	ypos += mheight/2;

	if (frontend_count) {
		for (int i = 0; i < frontend_count; i++) {
			CFrontend *fe = CFEManager::getInstance()->getFE(i);
			if (fe) {
				std::string s("Frontend ");
				s += to_string(i) + ": " + fe->getInfo()->name;
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, s, COL_MENUCONTENT_TEXT);
				ypos += mheight;
			}
		}
		ypos += mheight/2;
	}

	int headOffset=0;
	int mpOffset=0;
	const int headSize_mem = 5;
	const char *head_mem[headSize_mem] = {"Memory", "Size", "Used", "Available", "Use"};
	int offsets[] = {
		10,
		nameWidth + 10,
		nameWidth + 10 + (sizeWidth+10)*1,
		nameWidth + 10 + (sizeWidth+10)*2,
		nameWidth + 10 + (sizeWidth+10)*3,
	};
	int widths[] = { 0, sizeWidth, sizeWidth, sizeWidth, percWidth };

	// paint mem head
	for (int j = 0; j < headSize_mem; j++) {
		headOffset = offsets[j];
		int center = 0;
		if (j > 0)
			center = (widths[j] - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(head_mem[j], true))/2;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset + center, ypos+ mheight, width - 10, head_mem[j], COL_MENUCONTENTINACTIVE_TEXT);
	}
	ypos += mheight;

	for (int k = 0; k < 1 + have_swap; k++) {
		std::string tmp;
		memstat[k][DBINFO_USED] = memstat[k][DBINFO_TOTAL] - memstat[k][DBINFO_FREE];
		for (int j = 0; j < headSize_mem; j++) {
			switch (j) {
				case 0:
					tmp = memtype[k];
					break;
				case 1:
					tmp = bytes2string(1024 * memstat[k][DBINFO_TOTAL]);
					break;
				case 2:
					tmp = bytes2string(1024 * memstat[k][DBINFO_USED]);
					break;
				case 3:
					tmp = bytes2string(1024 * memstat[k][DBINFO_FREE]);
					break;
				case 4:
					tmp = to_string(memstat[k][DBINFO_TOTAL] ? (memstat[k][DBINFO_USED] * 100) / memstat[k][DBINFO_TOTAL] : 0) + "%";
					break;
			}
			mpOffset = offsets[j];
			int center = 0;
			if (j > 0)
				center = (widths[j] - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true))/2;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset + center, ypos+ mheight, width - 10, tmp, COL_MENUCONTENT_TEXT);
		}
		if (pbw > 8) /* smaller progressbar is not useful ;) */
		{
			CProgressBar pb(x+offsetw, ypos+3, pbw, mheight-10);
			pb.setBlink();
			pb.setInvert();
			pb.setValues(memstat[k][0] ? (memstat[k][1] * 100) / memstat[k][0] : 0, 100);
			pb.paint(false);
		}
		ypos += mheight;
	}
	ypos += mheight/2;
	
	const int headSize_mnt = 5;
	const char *head_mnt[headSize_mnt] = {"Filesystem", "Size", "Used", "Available", "Use"};
	// paint mount head
	for (int j = 0; j < headSize_mnt; j++) {
		headOffset = offsets[j];
		int center = 0;
		if (j > 0)
			center = (widths[j] - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(head_mnt[j], true))/2;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset + center, ypos+ mheight, width - 10, head_mnt[j], COL_MENUCONTENTINACTIVE_TEXT);
	}
	ypos += mheight;

	for (std::map<std::string, bool>::iterator it = mounts.begin(); it != mounts.end(); ++it) {
		struct statfs s;
		if (::statfs((*it).first.c_str(), &s) == 0) {
			if (s.f_blocks > 0) {
				int percent_used;
				uint64_t bytes_total;
				uint64_t bytes_used;
				uint64_t bytes_free;
				bytes_total = s.f_blocks * s.f_bsize;
				bytes_free  = s.f_bfree  * s.f_bsize;
				bytes_used = bytes_total - bytes_free;
				percent_used = (bytes_used * 200 + bytes_total) / 2 / bytes_total;
				//paint mountpoints
				for (int j = 0; j < headSize_mnt; j++) {
					std::string tmp;
					mpOffset = offsets[j];
					int _w = width;
					switch (j) {
					case 0:
						tmp = (*it).first;
						if (tmp == "/")
							tmp = "rootfs";
						_w = nameWidth - mpOffset;
						if ((*it).second)
							_w -= icon_w + 10;
						break;
					case 1:
						tmp = bytes2string(bytes_total, false);
						break;
					case 2:
						tmp = bytes2string(bytes_used, false);
						break;
					case 3:
						tmp = bytes2string(bytes_free, false);
						break;
					case 4:
						tmp = to_string(percent_used) + "%";
						break;
					}
					int center = 0;
					if (j > 0)
						center = (widths[j] - g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true))/2;
					g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset + center, ypos+ mheight, _w - 10, tmp, COL_MENUCONTENT_TEXT);
					if ((*it).second && icon_w>0 && icon_h>0)
						frameBuffer->paintIcon(NEUTRINO_ICON_REC, x + nameWidth - icon_w, ypos + (mheight/2 - icon_h/2));
				}
				if (pbw > 8) /* smaller progressbar is not useful ;) */
				{
					CProgressBar pb(x+offsetw, ypos+3, pbw, mheight-10);
					pb.setBlink();
					pb.setInvert();
					pb.setValues(percent_used, 100);
					pb.paint(false);
				}
				ypos += mheight;
			}
		}
		if (ypos > y + height - mheight)	/* the screen is not high enough */
			break;				/* todo: scrolling? */
	}
}

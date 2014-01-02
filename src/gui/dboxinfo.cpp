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

	fontWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	sizeWidth = 6 * g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getMaxDigitWidth()
		    + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(std::string(" MiB") + g_Locale->getText(LOCALE_UNIT_DECIMAL), true); ;//9999.99 MiB
	percWidth = 3 * g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getMaxDigitWidth()
		    + g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("%", true); //100%
	nameWidth = fontWidth * 17;
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
	height = hheight;
	height += mheight/2;	// space
	int cpuload_y0 = height;
	height += mheight;	// time
	height += mheight;	// uptime
	height += mheight;	// load
	int cpuload_y1 = height;
	height += mheight/2;	// space

	int frontend_count = CFEManager::getInstance()->getFrontendCount();
	if (frontend_count > 2)
		height += mheight * (frontend_count - 2);

	int icon_w = 0, icon_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icon_w, &icon_h);

#define MEMINFO_TOTAL 0
#define MEMINFO_USED 1
#define MEMINFO_FREE 2
#define MEMINFO_COLUMNS 3

#define MEMINFO_RAM 0
#define MEMINFO_SWAP 1
#define MEMINFO_ROWS 2
	unsigned long long memstat[MEMINFO_ROWS][MEMINFO_COLUMNS] = { { 0, 0, 0 }, { 0, 0, 0 } }; // total, used, free
	const char *memtype[MEMINFO_ROWS] = { "RAM", "Swap" };
	FILE *procmeminfo = fopen("/proc/meminfo", "r");
	if (procmeminfo) {
		char buf[80], a[80];
		long long unsigned v;
		while (fgets(buf, sizeof(buf), procmeminfo)) {
			char unit[10];
			*unit = 0;
			if ((3 == sscanf(buf, "%[^:]: %llu %s", a, &v, unit))
			 || (2 == sscanf(buf, "%[^:]: %llu", a, &v))) {
				if (*unit == 'k')
					v <<= 10;
				if (!strcasecmp(a, "MemTotal"))
					memstat[MEMINFO_RAM][MEMINFO_TOTAL] += v;
				else if (!strcasecmp(a, "MemFree"))
					memstat[MEMINFO_RAM][MEMINFO_FREE] += v;
				else if (!strcasecmp(a, "Inactive"))
					memstat[MEMINFO_RAM][MEMINFO_FREE] += v;
				else if (!strcasecmp(a, "SwapTotal"))
					memstat[MEMINFO_SWAP][MEMINFO_TOTAL] = v;
				else if (!strcasecmp(a, "SwapFree"))
					memstat[MEMINFO_SWAP][MEMINFO_FREE] += v;
			}
		}
		fclose(procmeminfo);
	}
	bool have_swap = memstat[MEMINFO_SWAP][MEMINFO_TOTAL];
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

	char ubuf_boot[80];
	struct sysinfo info;
	sysinfo(&info);
	now -= info.uptime;
	strftime(ubuf_boot, sizeof(ubuf_boot), "Boot: %FT%H:%M:%S%z", localtime(&now));

	int time_width = std::max(g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(ubuf), g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(ubuf_boot));

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + offsetw - time_width - 10, ypos+ mheight, time_width, ubuf, COL_MENUCONTENT_TEXT);
	ypos += mheight;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + offsetw - time_width - 10, ypos+ mheight, time_width, ubuf_boot, COL_MENUCONTENT_TEXT);
	ypos += mheight;

	if (data_last > -1) {
		snprintf(ubuf, sizeof(ubuf), "Load: %d%s%d%%", data_last/10, g_Locale->getText(LOCALE_UNIT_DECIMAL), data_last%10);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + offsetw - time_width - 10, ypos+ mheight, time_width, ubuf, COL_MENUCONTENT_TEXT);
	}
	ypos += mheight;

	int ypos_mem = ypos;

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

	ypos = y + hheight + mheight/2;

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + 10, ypos + mheight, width - 10, "Frontends", COL_MENUCONTENTINACTIVE_TEXT);
	ypos += mheight;
	for (int i = 0; i < frontend_count; i++) {
		CFrontend *fe = CFEManager::getInstance()->getFE(i);
		if (fe) {
			std::string s = to_string(i) + ": " + fe->getInfo()->name;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, s, COL_MENUCONTENT_TEXT);
			ypos += mheight;
		}
	}

	ypos = std::max(ypos, ypos_mem);
	ypos += mheight/2;

	int headOffset=0;
	int mpOffset=0;
	int offsets[] = {
		10,
		nameWidth + 10,
		nameWidth + 10 + (sizeWidth+10)*1,
		nameWidth + 10 + (sizeWidth+10)*2,
		nameWidth + 10 + (sizeWidth+10)*3,
	};
	int widths[] = { 0, sizeWidth, sizeWidth, sizeWidth, percWidth };

	const int headSize = 5;
	int maxWidth[headSize];
	memset(maxWidth, 0, headSize * sizeof(int));

	int ypos_mem_head = ypos;
	ypos += mheight;

	for (int row = 0; row < 1 + have_swap; row++) {
		std::string tmp;
		memstat[row][MEMINFO_USED] = memstat[row][MEMINFO_TOTAL] - memstat[row][MEMINFO_FREE];
		for (int column = 0; column < headSize; column++) {
			switch (column) {
				case 0:
					tmp = memtype[row];
					break;
				case 1:
					tmp = bytes2string(memstat[row][MEMINFO_TOTAL]);
					break;
				case 2:
					tmp = bytes2string(memstat[row][MEMINFO_USED]);
					break;
				case 3:
					tmp = bytes2string(memstat[row][MEMINFO_FREE]);
					break;
				case 4:
					tmp = to_string(memstat[row][MEMINFO_TOTAL] ? (memstat[row][MEMINFO_USED] * 100) / memstat[row][MEMINFO_TOTAL] : 0) + "%";
					break;
			}
			mpOffset = offsets[column];
			int space = 0;
			if (column > 0) {
				int rw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true);
				maxWidth[column] = std::max(maxWidth[column], rw);
				space = widths[column] - rw;
			}
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset + space, ypos+ mheight, width, tmp, COL_MENUCONTENT_TEXT);
		}
		if (pbw > 8) /* smaller progressbar is not useful ;) */
		{
			CProgressBar pb(x+offsetw, ypos+3, pbw, mheight-10);
			pb.setBlink();
			pb.setInvert();
			pb.setValues(memstat[row][MEMINFO_TOTAL] ? (memstat[row][MEMINFO_USED] * 100) / memstat[row][MEMINFO_TOTAL] : 0, 100);
			pb.paint(false);
		}
		ypos += mheight;
	}
	ypos += mheight/2;

	int ypos_mnt_head = ypos;
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
				int width_i = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth("i", true);
				//paint mountpoints
				for (int column = 0; column < headSize; column++) {
					std::string tmp;
					mpOffset = offsets[column];
					int _w = width;
					switch (column) {
					case 0:
						tmp = (*it).first;
						if (tmp == "/")
							tmp = "rootfs";
						_w = nameWidth - mpOffset;
						if ((*it).second)
							_w -= icon_w + 10;
						_w += width_i/2;
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
					int space = 0;
					if (column > 0) {
						int rw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(tmp, true);
						maxWidth[column] = std::max(maxWidth[column], rw);
						space = widths[column] - rw;
					}
					g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset + space, ypos+ mheight, _w, tmp, COL_MENUCONTENT_TEXT);
					if ((*it).second && icon_w>0 && icon_h>0)
						frameBuffer->paintIcon(NEUTRINO_ICON_REC, x + nameWidth - icon_w + width_i/2, ypos + (mheight/2 - icon_h/2));
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
	// paint mem head
	const char *head_mem[headSize] = {"Memory", "Size", "Used", "Available", "Use"};
	for (int column = 0; column < headSize; column++) {
		headOffset = offsets[column];
		int space = 0;
		if (column > 0) {
			int rw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(head_mem[column], true);
			if (rw > maxWidth[column])
				space = widths[column] - rw;
			else
				space = widths[column] - maxWidth[column] + (maxWidth[column] - rw)/2;
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset + space, ypos_mem_head + mheight, width, head_mem[column], COL_MENUCONTENTINACTIVE_TEXT);
	}
	// paint mount head
	const char *head_mnt[headSize] = {"Filesystem", "Size", "Used", "Available", "Use"};
	for (int column = 0; column < headSize; column++) {
		headOffset = offsets[column];
		int space = 0;
		if (column > 0) {
			int rw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(head_mnt[column], true);
			if (rw > maxWidth[column])
				space = widths[column] - rw;
			else
				space = widths[column] - maxWidth[column] + (maxWidth[column] - rw)/2;
		}
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset + space, ypos_mnt_head + mheight, width, head_mnt[column], COL_MENUCONTENTINACTIVE_TEXT);
	}
}

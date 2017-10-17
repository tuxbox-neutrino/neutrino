/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	(C) 2009-2011, 2013-2014 Stefan Seyfried

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


#include <global.h>
#include <neutrino.h>

#include <driver/abstime.h>
#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>
#include <driver/fade.h>
#include <driver/record.h>

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
	fm = g_Font[SNeutrinoSettings::FONT_TYPE_MENU];
	ft = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE];
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = ft->getHeight();
	mheight     = fm->getHeight();
	mheight += mheight & 1;
	width = 0;
	height = 0;
	x = 0;
	y = 0;
	header = NULL;
	fontWidth = fm->getWidth();
	sizeWidth = 6 * fm->getMaxDigitWidth()
		    + fm->getRenderWidth(std::string(" MiB") + g_Locale->getText(LOCALE_UNIT_DECIMAL)); ;//9999.99 MiB
	percWidth = 3 * fm->getMaxDigitWidth()
		    + fm->getRenderWidth("%"); //100%
	nameWidth = fontWidth * 17;
}

CDBoxInfoWidget::~CDBoxInfoWidget()
{
	delete header;
	header = NULL;
}

int CDBoxInfoWidget::exec(CMenuTarget* parent, const std::string &)
{
	if (parent)
	{
		parent->hide();
	}
	COSDFader fader(g_settings.theme.menu_Content_alpha);
	fader.StartFadeIn();

	paint();
	frameBuffer->blit();

	//int res = g_RCInput->messageLoop();
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = menu_return::RETURN_REPAINT;

	bool doLoop = true;

	int timeout = g_settings.timing[SNeutrinoSettings::TIMING_MENU];
	uint64_t timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
	uint32_t updateTimer = g_RCInput->addTimer(5*1000*1000, false);

	while (doLoop)
	{
		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		if((msg == NeutrinoMessages::EVT_TIMER) && (data == fader.GetFadeTimer())) {
			if(fader.FadeDone())
				doLoop = false;
		}
		else if((msg == NeutrinoMessages::EVT_TIMER) && (data == updateTimer)) {
			paint();
		}
		else if ( ( msg == CRCInput::RC_timeout ) ||
				( msg == CRCInput::RC_home ) ||
				( msg == CRCInput::RC_ok ) ) {
			if(fader.StartFadeOut()) {
				timeoutEnd = CRCInput::calcTimeoutEnd(1);
				msg = 0;
			} else
				doLoop = false;
		}
		else if(msg == CRCInput::RC_setup) {
			res = menu_return::RETURN_EXIT_ALL;
			doLoop = false;
		}
		else if(CNeutrinoApp::getInstance()->listModeKey(msg)) {
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
					timeoutEnd = CRCInput::calcTimeoutEnd(timeout);
				}
			}
		}
		frameBuffer->blit();
	}

	hide();
	fader.StopFade();
	g_RCInput->killTimer(updateTimer);
	return res;
}

void CDBoxInfoWidget::hide()
{
	header->kill();
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
		snprintf(result, sizeof(result), "%" PRIu64 "%s%02" PRIu64 " ", b, g_Locale->getText(LOCALE_UNIT_DECIMAL),
			(bytes - b * factor) * 100 / factor);
	else // no need for fractions for larger numbers
		snprintf(result, sizeof(result), "%" PRIu64 " ", b);

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
	height += mheight;	// boot time
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
	const char *memtype[MEMINFO_ROWS] = { g_Locale->getText(LOCALE_EXTRA_DBOXINFO_RAM), g_Locale->getText(LOCALE_EXTRA_DBOXINFO_SWAP) };
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
			size_t firstslash = line.find_first_of('/');
			size_t firstspace = line.find_first_of(' ');
			if ( (firstspace != std::string::npos && firstslash != std::string::npos && firstslash < firstspace) || (line.find("rootfs") == 0) ) {
				firstspace++;
				size_t nextspace = line.find_first_of(' ', firstspace);
				if (nextspace == std::string::npos || line.find("nodev", nextspace + 1) != std::string::npos)
					continue;
				std::string mountpoint = line.substr(firstspace, nextspace - firstspace);
				struct stat st;
				if (stat(mountpoint.c_str(), &st) || (seen.find(st.st_dev) != seen.end()))
					continue;
				seen[st.st_dev] = mountpoint;
				bool is_rec = (st.st_dev == rec_st.st_dev);
				mounts[mountpoint] = is_rec;
				int icon_space = is_rec ? 10 + icon_w : 0;
				nameWidth = std::max(nameWidth, fm->getRenderWidth(mountpoint, true) + icon_space + 10);
			}
		}
		in.close();
	}
	int satWidth = nameWidth;
	for (int i = 0; i < frontend_count; i++) {
		CFrontend *fe = CFEManager::getInstance()->getFE(i);
		if (fe) {
			std::string s = to_string(i) + ": " + fe->getName();
			satWidth = std::max(satWidth, fm->getRenderWidth(s));
		}
	}

	height += mheight;			// header
	height += mounts.size() * mheight;	// file systems
	height += mheight/2;			// space

	//int offsetw = satWidth+ (sizeWidth+10)*3 +10+percWidth+10;
	int offsetw = satWidth+ (sizeWidth)*3 +percWidth;
	width = offsetw + 10 + 120;

	int diff = frameBuffer->getScreenWidth() - width;
	if (diff < 0) {
		width += diff;
		offsetw += diff;
		nameWidth += diff;
	}
	height = h_max(height, 0);
	y = getScreenStartY(height);

	// fprintf(stderr, "CDBoxInfoWidget::CDBoxInfoWidget() x = %d, y = %d, width = %d height = %d\n", x, y, width, height);

	int ypos=y;

	//paint head
	std::string title(g_Locale->getText(LOCALE_EXTRA_DBOXINFO));
#if 0
	std::map<std::string,std::string> cpuinfo;
	in.open("/proc/cpuinfo");
	if (in.is_open()) {
		std::string line;
		while (getline(in, line)) {
			size_t colon = line.find_first_of(':');
			if (colon != std::string::npos && colon > 1) {
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
#endif
	title += ": ";
	title += g_info.hw_caps->boxvendor;
	title += " ";
	title += g_info.hw_caps->boxname;
	width = std::max(width, g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(title, true) + 50);
	x = getScreenStartX(width);

	if (!header)
		header = new CComponentsHeader(x, ypos, width, hheight, title, NEUTRINO_ICON_SHELL);
	if (!header->isPainted())
		header->paint(CC_SAVE_SCREEN_NO);

	//paint body
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	ypos += hheight + mheight/2;

	cSysLoad *sysload = cSysLoad::getInstance();
	int data_last = sysload->data_last;

	std::string str_now_title(g_Locale->getText(LOCALE_EXTRA_DBOXINFO_TIME));
	str_now_title += ": ";
	std::string str_boot_title(g_Locale->getText(LOCALE_EXTRA_DBOXINFO_BOOTTIME));
	str_boot_title += ": ";
	std::string str_up_title(g_Locale->getText(LOCALE_EXTRA_DBOXINFO_UPTIME));
	str_up_title += ": ";

	int time_title_width = std::max(fm->getRenderWidth(str_now_title, true), fm->getRenderWidth(str_boot_title));
	time_title_width = std::max(time_title_width, fm->getRenderWidth(str_up_title));

	time_t now = time(NULL);
	std::string str_now(strftime(g_Locale->getText(LOCALE_EXTRA_DBOXINFO_TIMEFORMAT), now));
	struct sysinfo info;
	sysinfo(&info);
	now -= info.uptime;
	std::string str_boot(strftime(g_Locale->getText(LOCALE_EXTRA_DBOXINFO_TIMEFORMAT), now));

	char ubuf[256] = { 0 };
	char sbuf[256] = { 0 };
        int updays, uphours, upminutes;

        updays = (int) info.uptime / (60*60*24);
        upminutes = (int) info.uptime / 60;
        uphours = (upminutes / 60) % 24;
        upminutes %= 60;

        if (updays) {
                snprintf(ubuf, sizeof(ubuf), "%d day%s, ", updays, (updays != 1) ? "s" : "");
                strcat(sbuf, ubuf);
        }
        if (uphours) {
                snprintf(ubuf, sizeof(ubuf), "%d hour%s, ", uphours, (uphours != 1) ? "s" : "");
                strcat(sbuf, ubuf);
        }
        snprintf(ubuf,sizeof(ubuf), "%d minute%s", upminutes, (upminutes != 1) ? "s" : "");
        strcat(sbuf, ubuf);

	snprintf(ubuf, sizeof(ubuf), "%s: ", g_Locale->getText(LOCALE_EXTRA_DBOXINFO_LOAD));
	int time_width = fm->getRenderWidth(ubuf);
	time_width = std::max(time_width, fm->getRenderWidth(str_now));
	time_width = std::max(time_width, fm->getRenderWidth(str_boot));
	time_width = std::max(time_width, fm->getRenderWidth(sbuf));

	int time_width_total = time_title_width + time_width;

	// boot time
	fm->RenderString(x + offsetw - time_width_total - 10, ypos + mheight, time_title_width, str_boot_title, COL_MENUCONTENTINACTIVE_TEXT);
	fm->RenderString(x + offsetw - time_width_total - 10 + time_title_width, ypos + mheight, time_width, str_boot, COL_MENUCONTENT_TEXT);
	ypos += mheight;
	// time now
	fm->RenderString(x + offsetw - time_width_total - 10, ypos + mheight, time_title_width, str_now_title, COL_MENUCONTENTINACTIVE_TEXT);
	fm->RenderString(x + offsetw - time_width_total - 10 + time_title_width, ypos + mheight, time_width, str_now, COL_MENUCONTENT_TEXT);
	ypos += mheight;
        // paint uptime
	fm->RenderString(x + offsetw - time_width_total - 10, ypos + mheight, time_title_width, str_up_title, COL_MENUCONTENTINACTIVE_TEXT);
	fm->RenderString(x + offsetw - time_width_total - 10 + time_title_width, ypos + mheight, time_width, sbuf, COL_MENUCONTENT_TEXT);
        ypos += mheight;

	if (data_last > -1) {
		fm->RenderString(x + offsetw - time_width_total - 10, ypos + mheight, time_title_width, ubuf, COL_MENUCONTENTINACTIVE_TEXT);
		snprintf(ubuf, sizeof(ubuf), "%d%s%d%%", data_last/10, g_Locale->getText(LOCALE_UNIT_DECIMAL), data_last%10);
		fm->RenderString(x + offsetw - time_width_total - 10 + time_title_width, ypos + mheight, time_width, ubuf, COL_MENUCONTENT_TEXT);
	}
	ypos += mheight;

	int ypos_mem = ypos;
	int pbw_fix = 0;
	int pbw = width - offsetw - 10;
	if (pbw > 8) /* smaller progressbar is not useful ;) */
	{
		unsigned int h = cpuload_y1 - cpuload_y0;
		cpuload_y0 += y;
		cpuload_y1 += y;
		frameBuffer->paintBoxRel(x + offsetw, cpuload_y0, pbw, h, COL_MENUCONTENT_PLUS_2);

		int off = std::max(0, (int)sysload->data_avail - pbw);
		for (unsigned int i = 0; i < sysload->data_avail - off; i++) {
			if ((sysload->data[i + off] * h / 1000) > 0)
				frameBuffer->paintVLine(x+offsetw + i, cpuload_y1 - sysload->data[i + off] * h / 1000, cpuload_y1, COL_MENUCONTENT_PLUS_7);
		}
	}

	ypos = y + hheight + mheight/2;

        fm->RenderString(x + 10, ypos + mheight, width - 10, g_Locale->getText(LOCALE_EXTRA_DBOXINFO_FRONTEND), COL_MENUCONTENTINACTIVE_TEXT);
	ypos += mheight;
	for (int i = 0; i < frontend_count; i++) {
		CFrontend *fe = CFEManager::getInstance()->getFE(i);
		if (fe) {
			std::string s = to_string(i) + ": " + fe->getName();
			fm->RenderString(x+ 10, ypos+ mheight, width - 10, s, COL_MENUCONTENT_TEXT);
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
				int rw = fm->getRenderWidth(tmp);
				maxWidth[column] = std::max(maxWidth[column], rw);
				space = widths[column] - rw;
                               if( (mpOffset + rw + space) > offsetw)
					pbw_fix =  ((mpOffset + rw + space + 10) - offsetw);
			}
			fm->RenderString(x + mpOffset + space, ypos+ mheight, width, tmp, COL_MENUCONTENT_TEXT);
		}
		if (pbw-pbw_fix > 8) /* smaller progressbar is not useful ;) */
		{
			CProgressBar pb(x+offsetw+pbw_fix, ypos+mheight/4, pbw-pbw_fix, mheight/2);
			pb.setType(CProgressBar::PB_REDRIGHT);
			pb.setValues(memstat[row][MEMINFO_TOTAL] ? (memstat[row][MEMINFO_USED] * 100) / memstat[row][MEMINFO_TOTAL] : 0, 100);
			pb.paint(false);
		}
		ypos += mheight;
	}
	ypos += mheight/2;

	int ypos_mnt_head = ypos;
	ypos += mheight;

	int width_i = fm->getRenderWidth("i");
	CRecordManager * crm		= CRecordManager::getInstance();

	for (std::map<std::string, bool>::iterator it = mounts.begin(); it != mounts.end(); ++it) {
		struct statfs s;
		if (::statfs((*it).first.c_str(), &s) == 0) {
			if (s.f_blocks > 0) {
				uint64_t bytes_total = s.f_blocks * s.f_bsize;
				uint64_t bytes_free  = s.f_bfree  * s.f_bsize;
				uint64_t bytes_used = bytes_total - bytes_free;
				int percent_used = (bytes_used * 200 + bytes_total) / 2 / bytes_total;
				//paint mountpoints
				for (int column = 0; column < headSize; column++) {
					std::string tmp;
					mpOffset = offsets[column];
					const char *mnt;
					int _w = width;
					switch (column) {
					case 0:
						tmp = (*it).first;
						if (tmp == "/")
							tmp = "rootfs";
						mnt = tmp.c_str();
						tmp = basename((char *)mnt);
						_w = nameWidth - mpOffset;
						if ((*it).second)
							_w -= icon_w;
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
						int rw = fm->getRenderWidth(tmp);
						maxWidth[column] = std::max(maxWidth[column], rw);
						space = widths[column] - rw;
					}
					fm->RenderString(x + mpOffset + space, ypos+ mheight, _w, tmp, COL_MENUCONTENT_TEXT);
					if ((*it).second && icon_w>0 && icon_h>0)
						frameBuffer->paintIcon(crm->RecordingStatus() ? NEUTRINO_ICON_REC:NEUTRINO_ICON_REC_GRAY, x + nameWidth - icon_w + width_i/2, ypos + (mheight/2 - icon_h/2));
				}
				if (pbw-pbw_fix > 8) /* smaller progressbar is not useful ;) */
				{
					CProgressBar pb(x+offsetw+pbw_fix, ypos+mheight/4, pbw-pbw_fix, mheight/2);
					pb.setType(CProgressBar::PB_REDRIGHT);
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
	const char *head_mem[headSize] = {
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_MEMORY),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_SIZE),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_USED),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_AVAILABLE),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_USE)
	};
	int len4 = fm->getRenderWidth(head_mem[4]);
	if (pbw > 8 && maxWidth[4] < len4) {
		maxWidth[4] += 10 + pbw;
		maxWidth[4] = std::min(maxWidth[4], len4);
		widths[4] = maxWidth[4];
	}
	for (int column = 0; column < headSize; column++) {
		headOffset = offsets[column];
		int space = 0;
		if (column > 0) {
			int rw = fm->getRenderWidth(head_mem[column]);
			if (rw > maxWidth[column])
				space = widths[column] - rw;
			else
				space = widths[column] - maxWidth[column] + (maxWidth[column] - rw)/2;
		}
		fm->RenderString(x + headOffset + space, ypos_mem_head + mheight, width, head_mem[column], COL_MENUCONTENTINACTIVE_TEXT);
	}
	// paint mount head
	const char *head_mnt[headSize] = {
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_FILESYSTEM),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_SIZE),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_USED),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_AVAILABLE),
		g_Locale->getText(LOCALE_EXTRA_DBOXINFO_USE)
	};
	for (int column = 0; column < headSize; column++) {
		headOffset = offsets[column];
                int space = 0;
                if (column > 0) {
			int rw = fm->getRenderWidth(head_mnt[column]);
			if (rw > maxWidth[column])
				space = widths[column] - rw;
			else
				space = widths[column] - maxWidth[column] + (maxWidth[column] - rw)/2;
                }
		fm->RenderString(x + headOffset + space, ypos_mnt_head + mheight, width, head_mnt[column], COL_MENUCONTENTINACTIVE_TEXT);
	}
}

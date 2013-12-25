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
#include <mntent.h>

#include <gui/dboxinfo.h>
#include <gui/components/cc.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>
#include <driver/fade.h>

#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <system/helpers.h>
#include <map>
#include <ctype.h>

static const int FSHIFT = 16;              /* nr of bits of precision */
#define FIXED_1         (1<<FSHIFT)     /* 1.0 as fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

CDBoxInfoWidget::CDBoxInfoWidget()
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	// width       = 600;
	// height      = hheight+13*mheight+ 10;
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
	}

	hide();
	fader.Stop();
	g_RCInput->killTimer(updateTimer);
	return res;
}

void CDBoxInfoWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
}

static void bytes2string(uint64_t bytes, char *result, size_t len)
{
	static const char units[] = " kMGT";
	unsigned int magnitude = 0;
	uint64_t factor = 1;
	uint64_t b = bytes;

	while ((b > 1024) && (magnitude < strlen(units) - 1))
	{
		magnitude++;
		b /= 1024;
		factor *= 1024;
	}
	if (b < 1024) /* no need for fractions for big numbers */
		snprintf(result, len, "%3d.%02d%c", (int)b,
			(int)((bytes - b * factor) * 100 / factor), units[magnitude]);
	else
		snprintf(result, len, "%d%c", (int)bytes, units[magnitude]);

	result[len - 1] = '\0';
	//printf("b2s: b:%lld r:'%s' mag:%d u:%d\n", bytes, result, magnitude, (int)strlen(units));
}

void CDBoxInfoWidget::paint()
{
	const int headSize = 5;
	const char *head[headSize] = {"Filesystem", "Size", "Used", "Available", "Use%"};
	int fontWidth = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getWidth();
	int sizeOffset = fontWidth * 7;//9999.99M
	int percOffset = fontWidth * 3 ;//100%
	int nameOffset = fontWidth * 17;//WWWwwwwwww
	height = hheight + 8 * mheight;

	int icon_w = 0, icon_h = 0;
	frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icon_w, &icon_h);

	struct statfs s;
	FILE *          mountFile;
	struct mntent * mnt;

	/* this is lame, as it duplicates code. OTOH, it is small and fast enough...
	   The algorithm is exactly the same as below in the display routine */
	if ((mountFile = setmntent("/proc/mounts", "r")) == NULL) {
		perror("/proc/mounts");
	} else {
		map<dev_t,bool>seen;
		while ((mnt = getmntent(mountFile)) != NULL) {
			if (strcmp(mnt->mnt_fsname, "rootfs") == 0)
				continue;
			if (::statfs(mnt->mnt_dir, &s) == 0) {
				struct stat st;
				if (!stat(mnt->mnt_dir, &st) && seen.find(st.st_dev) != seen.end())
					continue;
				seen[st.st_dev] = true;
				switch (s.f_type)	/* f_type is long */
				{
				case 0xEF53L:		/*EXT2 & EXT3*/
				case 0x6969L:		/*NFS*/
				case 0xFF534D42L:	/*CIFS*/
				case 0x517BL:		/*SMB*/
				case 0x52654973L:	/*REISERFS*/
				case 0x65735546L:	/*fuse for ntfs*/
				case 0x58465342L:	/*xfs*/
				case 0x4d44L:		/*msdos*/
				case 0x72b6L:		/*jffs2*/
				case 0x5941ff53L:	/*yaffs2*/
					break;
				default:
					continue;
				}
				height += mheight;
			}
			nameOffset = std::max(nameOffset, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(basename(mnt->mnt_dir), true) + icon_w + 20);
		}
		endmntent(mountFile);
	}

	int m[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } }; // size, used, available
#define DBINFO_TOTAL 0
#define DBINFO_USED 1
#define DBINFO_FREE 2
#define DBINFO_RAM 0
#define DBINFO_SWAP 1
	const char *n[2] = { "RAM", "Swap" };
	FILE *procmeminfo = fopen("/proc/meminfo", "r");
	if (procmeminfo) {
		char buf[80], a[80];
		int v;
		while (fgets(buf, sizeof(buf), procmeminfo))
			if (2 == sscanf(buf, "%[^:]: %d", a, &v)) {
				if (!strcasecmp(a, "MemTotal"))
					m[DBINFO_RAM][DBINFO_TOTAL] += v;
				else if (!strcasecmp(a, "MemFree"))
					m[DBINFO_RAM][DBINFO_FREE] += v;
				else if (!strcasecmp(a, "Inactive"))
					m[DBINFO_RAM][DBINFO_FREE] += v;
				else if (!strcasecmp(a, "SwapTotal"))
					m[DBINFO_SWAP][DBINFO_TOTAL] = v;
				else if (!strcasecmp(a, "SwapFree"))
					m[DBINFO_SWAP][DBINFO_FREE] += v;
			}
		fclose(procmeminfo);
	}
	bool have_swap = m[DBINFO_SWAP][DBINFO_TOTAL];

	if (have_swap)
		height += mheight;

	int offsetw = nameOffset+ (sizeOffset+10)*3 +10+percOffset+10;
	offsetw += 20;
	width = offsetw + 10 + 120;

	int _width = w_max(width, 0);
	if (_width < width) {
		int diff = width - _width;
		width = _width;
		offsetw -= diff;
		nameOffset -= width;
	}
	height = h_max(height, 0);
	x = getScreenStartX(width);
	y = getScreenStartY(height);

	// fprintf(stderr, "CDBoxInfoWidget::CDBoxInfoWidget() x = %d, y = %d, width = %d height = %d\n", x, y, width, height);

	int ypos=y;
	int i = 0;
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, RADIUS_LARGE, CORNER_TOP);
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, RADIUS_LARGE, CORNER_BOTTOM);

	//paint menu head
	string iconfile = NEUTRINO_ICON_SHELL;
	int HeadiconOffset = 0;
	if(!(iconfile.empty())){
		int w, h;
		frameBuffer->getIconSize(iconfile.c_str(), &w, &h);
		HeadiconOffset = w+6;
	}
	int fw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getWidth();
	int binfo_w = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth(g_Locale->getText(LOCALE_EXTRA_DBOXINFO));

	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+(fw/3)+HeadiconOffset,y+hheight+1,
		width-((fw/3)+HeadiconOffset), g_Locale->getText(LOCALE_EXTRA_DBOXINFO),
		COL_MENUHEAD_TEXT, 0, true); // UTF-8
	frameBuffer->paintIcon(iconfile, x + fw/4, y, hheight);

	ypos+= hheight + (mheight >>1);
	FILE* fd = fopen("/proc/cpuinfo", "rt");
	if (fd==NULL) {
		printf("error while opening proc-cpuinfo\n" );
	} else {
		char *buffer=NULL;
		size_t len = 0;
		ssize_t read;
		while ((read = getline(&buffer, &len, fd)) != -1) {
#if HAVE_TRIPLEDRAGON
			if (!(strncmp(const_cast<char *>("machine"),buffer,7)))
#else
			if (!(strncmp(const_cast<char *>("Hardware"),buffer,8)))
#endif
			{
				char *t=rindex(buffer,'\n');
				if (t)
					*t='\0';

				std::string hw;
				char *p=rindex(buffer,':');
				if (p)
					hw=++p;
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10+binfo_w+HeadiconOffset, y + hheight+1, width - 10, hw.c_str(), COL_MENUHEAD_TEXT, 0, true); // UTF-8
				break;
			}
			i++;
#if HAVE_TRIPLEDRAGON
			if (i == 1 || i > 3)
#else
			if (i > 2)
#endif
				continue;
			if (read > 0 && buffer[read-1] == '\n')
				buffer[read-1] = '\0';
			buffer[0] = toupper(buffer[0]);
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, buffer, COL_MENUCONTENT_TEXT);
			ypos+= mheight;
		}
		fclose(fd);
		if (buffer)
			free(buffer);
	}
	char *ubuf=NULL, *sbuf=NULL;
	int buf_size=256;
	ubuf = new char[buf_size];
	sbuf = new char[buf_size];

	if (sbuf != NULL && ubuf != NULL) {
		int updays, uphours, upminutes;
		struct sysinfo info;
		struct tm *current_time;
		time_t current_secs;
		memset(sbuf, 0, 256);
		time(&current_secs);
		current_time = localtime(&current_secs);

		sysinfo(&info);

		snprintf( ubuf,buf_size, "Uptime: %2d:%02d%s  up ",
			  current_time->tm_hour%12 ? current_time->tm_hour%12 : 12,
			  current_time->tm_min, current_time->tm_hour > 11 ? "pm" : "am");
		strcat(sbuf, ubuf);
		updays = (int) info.uptime / (60*60*24);
		if (updays) {
			snprintf(ubuf,buf_size, "%d day%s, ", updays, (updays != 1) ? "s" : "");
			strcat(sbuf, ubuf);
		}
		upminutes = (int) info.uptime / 60;
		uphours = (upminutes / 60) % 24;
		upminutes %= 60;
		if (uphours)
			snprintf(ubuf,buf_size,"%2d:%02d, ", uphours, upminutes);
		else
			snprintf(ubuf,buf_size,"%d min, ", upminutes);
		strcat(sbuf, ubuf);

		snprintf(ubuf,buf_size, "load: %ld.%02ld, %ld.%02ld, %ld.%02ld",
			 LOAD_INT(info.loads[0]), LOAD_FRAC(info.loads[0]),
			 LOAD_INT(info.loads[1]), LOAD_FRAC(info.loads[1]),
			 LOAD_INT(info.loads[2]), LOAD_FRAC(info.loads[2]));
		strcat(sbuf, ubuf);
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width - 10, sbuf, COL_MENUCONTENT_TEXT);
		ypos+= mheight/2;
		ypos+= mheight;
		int headOffset=0;
		int mpOffset=0;
		bool rec_mp=false, memory_flag = false;
		const int headSize_mem = 5;
		const char *head_mem[headSize_mem] = {"Memory", "Size", "Used", "Available", "Use%"};
		// paint mount head
		for (int j = 0; j < headSize_mem; j++) {
			switch (j)
			{
				case 0:
				headOffset = 10;
				break;
				case 1:
				headOffset = nameOffset + 20;
				break;
				case 2:
				headOffset = nameOffset + sizeOffset+10 +20;
				break;
				case 3:
				headOffset = nameOffset + (sizeOffset+10)*2+15;
				break;
				case 4:
				headOffset = nameOffset + (sizeOffset+10)*3+15;
				break;
			}
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset, ypos+ mheight, width - 10, head_mem[j], COL_MENUCONTENTINACTIVE_TEXT);
		}
		ypos+= mheight;

		for (int k = 0; k < 1 + have_swap; k++) {
			m[k][DBINFO_USED] = m[k][DBINFO_TOTAL] - m[k][DBINFO_FREE];
			for (int j = 0; j < headSize_mem; j++) {
				switch (j) {
					case 0:
						mpOffset = 10;
						snprintf(ubuf,buf_size,"%-20.20s", n[k]);
						break;
					case 1:
						mpOffset = nameOffset + 10;
						bytes2string(1024 * m[k][DBINFO_TOTAL], ubuf, buf_size);
						break;
					case 2:
						mpOffset = nameOffset+ (sizeOffset+10)*1+10;
						bytes2string(1024 * m[k][DBINFO_FREE], ubuf, buf_size);
						break;
					case 3:
						mpOffset = nameOffset+ (sizeOffset+10)*2+10;
						bytes2string(1024 * m[k][DBINFO_USED], ubuf, buf_size);
						break;
					case 4:
						mpOffset = nameOffset+ (sizeOffset+10)*3+10;
						snprintf(ubuf, buf_size, "%4d%%", m[k][DBINFO_TOTAL] ? (m[k][DBINFO_FREE] * 100) / m[k][DBINFO_TOTAL] : 0);
						break;
				}
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset, ypos+ mheight, width - 10, ubuf, COL_MENUCONTENT_TEXT);
			}
			int pbw = width - offsetw - 10;
			if (pbw > 8) /* smaller progressbar is not useful ;) */
			{
				CProgressBar pb(x+offsetw, ypos+3, pbw, mheight-10);
				pb.setBlink();
				pb.setInvert();
				pb.setValues(m[k][0] ? (m[k][1] * 100) / m[k][0] : 0, 100);
				pb.paint(false);
			}
			ypos+= mheight;
		}
		ypos+= mheight;
		
		// paint mount head
		for (int j = 0; j < headSize; j++) {
			switch (j)
			{
				case 0:
				headOffset = 10;
				break;
				case 1:
				headOffset = nameOffset + 20;
				break;
				case 2:
				headOffset = nameOffset + sizeOffset+10 +20;
				break;
				case 3:
				headOffset = nameOffset + (sizeOffset+10)*2+15;
				break;
				case 4:
				headOffset = nameOffset + (sizeOffset+10)*3+15;
				break;
			}
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ headOffset, ypos+ mheight, width - 10, head[j], COL_MENUCONTENTINACTIVE_TEXT);
		}
		ypos+= mheight;

		if ((mountFile = setmntent("/proc/mounts", "r")) == 0 ) {
			perror("/proc/mounts");
		}
		else {
			map<dev_t,bool>seen;
			while ((mnt = getmntent(mountFile)) != 0) {
				if (::statfs(mnt->mnt_dir, &s) == 0) {

					struct stat st;
					if (!stat(mnt->mnt_dir, &st) && seen.find(st.st_dev) != seen.end())
						continue;
					seen[st.st_dev] = true;
					switch (s.f_type) {
						case (int) 0xEF53:      /*EXT2 & EXT3*/
						case (int) 0x6969:      /*NFS*/
						case (int) 0xFF534D42:  /*CIFS*/
						case (int) 0x517B:      /*SMB*/
						case (int) 0x52654973:  /*REISERFS*/
						case (int) 0x65735546:  /*fuse for ntfs*/
						case (int) 0x58465342:  /*xfs*/
						case (int) 0x4d44:      /*msdos*/
						case (int) 0x72b6:	/*jffs2*/
						case (int) 0x5941ff53:	/*yaffs2*/
							break;
						default:
							continue;
					}
					if ( s.f_blocks > 0 || memory_flag ) {
						int percent_used;
						uint64_t bytes_total;
						uint64_t bytes_used;
						uint64_t bytes_free;
						if (memory_flag)
						{
							//bytes_total = info.totalram;
							//bytes_free  = info.freeram;
							unsigned long t, f;
							get_mem_usage(t, f);
							bytes_total = t*1024;
							bytes_free = f*1024;
						}
						else
						{
							bytes_total = s.f_blocks * s.f_bsize;
							bytes_free  = s.f_bfree  * s.f_bsize;
						}
						bytes_used = bytes_total - bytes_free;
						percent_used = (bytes_used * 200 + bytes_total) / 2 / bytes_total;
						struct statfs rec_s;
						if (statfs(g_settings.network_nfs_recordingdir.c_str(), &rec_s))
							memset(&rec_s, 0, sizeof(rec_s));
						//paint mountpoints
						for (int j = 0; j < headSize; j++) {
							int _w = width;
							switch (j) {
							case 0:
								rec_mp = !memcmp(&s.f_fsid, &rec_s.f_fsid, sizeof(s.f_fsid)) && (s.f_type != 0x72b6) && (s.f_type != 0x5941ff53);
								mpOffset = 10;
								strncpy(ubuf, basename(mnt->mnt_dir), buf_size);
								_w = nameOffset - mpOffset;
								if (rec_mp)
									_w -= icon_w;
								break;
							case 1:
								mpOffset = nameOffset + 10;
								bytes2string(bytes_total, ubuf, buf_size);
								break;
							case 2:
								mpOffset = nameOffset+ (sizeOffset+10)*1+10;
								bytes2string(bytes_used, ubuf, buf_size);
								break;
							case 3:
								mpOffset = nameOffset+ (sizeOffset+10)*2+10;
								bytes2string(bytes_free, ubuf, buf_size);
								break;
							case 4:
								mpOffset = nameOffset+ (sizeOffset+10)*3+10;
								snprintf(ubuf, buf_size, "%4d%%", percent_used);
								break;
							}
							g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x + mpOffset, ypos+ mheight, _w - 10, ubuf, COL_MENUCONTENT_TEXT);
							if (rec_mp) {
								if (icon_w>0 && icon_h>0)
									frameBuffer->paintIcon(NEUTRINO_ICON_REC, x + nameOffset - 10 - icon_w, ypos + (mheight/2 - icon_h/2));
								rec_mp = false;
							}
						}
						int pbw = width - offsetw - 10;
						memory_flag = false;
//fprintf(stderr, "width: %d offsetw: %d pbw: %d\n", width, offsetw, pbw);
						if (pbw > 8) /* smaller progressbar is not useful ;) */
						{
							CProgressBar pb(x+offsetw, ypos+3, pbw, mheight-10);
							pb.setBlink();
							pb.setInvert();
							pb.setValues(percent_used, 100);
							pb.paint(false);
						}
						ypos+= mheight;
					}
					i++;
				}
				if (ypos > y + height - mheight)	/* the screen is not high enough */
					break;				/* todo: scrolling? */
			}
			endmntent(mountFile);
		}
	}
	if (sbuf)
		delete[] sbuf;
	if (ubuf)
		delete[] ubuf;

}

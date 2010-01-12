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
#include <gui/widget/progressbar.h>

#include <global.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>

#include <sys/sysinfo.h>
#include <sys/vfs.h>
extern bool pb_blink;

static const int FSHIFT = 16;              /* nr of bits of precision */
#define FIXED_1         (1<<FSHIFT)     /* 1.0 as fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)
#define ROUND_RADIUS 9
CDBoxInfoWidget::CDBoxInfoWidget()
{
	frameBuffer = CFrameBuffer::getInstance();
	hheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();
	mheight     = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
	// width       = 600;
	// height      = hheight+13*mheight+ 10;
	width  = w_max (600, 0);
	height = h_max (hheight+14*mheight+ 10, 0);

	x=getScreenStartX( width );
	y=getScreenStartY( height );
}


int CDBoxInfoWidget::exec(CMenuTarget* parent, const std::string &)
{
	if (parent)
	{
		parent->hide();
	}
	paint();

	int res = g_RCInput->messageLoop();

	hide();
	return res;
}

void CDBoxInfoWidget::hide()
{
	frameBuffer->paintBackgroundBoxRel(x,y, width,height);
}
#define FP_IOCTL_GET_LNB_CURRENT 0x100
#define FP_IOCTL_GETID          0
#define FP_IOCTL_UPGRADE_CTRL    0x200

void CDBoxInfoWidget::paint()
{
	int ypos=y;
	int tmpypos=ypos;
	int i = 0;
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, CORNER_TOP);
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, CORNER_BOTTOM);

	ypos+= hheight + (mheight >>1);
	FILE* fd = fopen("/proc/cpuinfo", "rt");
	if (fd==NULL) {
		printf("error while opening proc-cpuinfo\n" );
	} else {
		char *buffer=NULL;
		size_t len = 0;
		ssize_t read;
		while ((read = getline(&buffer, &len, fd)) != -1) {
			if (!(strncmp(const_cast<char *>("Hardware"),buffer,8))) {
				char *t=rindex(buffer,'\n');
				if(t)
					*t='\0';

				std::string hw;
				char *p=rindex(buffer,':');
				if(p)
					hw=++p;
				hw+=" Info";
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10, tmpypos+ hheight+1, width, hw.c_str(), COL_MENUHEAD, 0, true); // UTF-8
				break;
			}
			i++;
			if (i > 2)
				continue;
			g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, buffer, COL_MENUCONTENT);
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

		snprintf( ubuf,buf_size, "%2d:%02d%s  up ",
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

		snprintf(ubuf,buf_size, "load: %ld.%02ld, %ld.%02ld, %ld.%02ld\n",
			 LOAD_INT(info.loads[0]), LOAD_FRAC(info.loads[0]),
			 LOAD_INT(info.loads[1]), LOAD_FRAC(info.loads[1]),
			 LOAD_INT(info.loads[2]), LOAD_FRAC(info.loads[2]));
		strcat(sbuf, ubuf);
		ypos+= mheight/2;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, sbuf, COL_MENUCONTENT);
		ypos+= mheight;

		snprintf(ubuf,buf_size, "memory total %dKb, free %dKb", (int) info.totalram/1024, (int) info.freeram/1024);
		ypos+= mheight/2;
		g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, ubuf, COL_MENUCONTENT);
		ypos+= mheight;
		struct statfs s;

		FILE *          mountFile;
		struct mntent * mnt;

		if ((mountFile = setmntent("/proc/mounts", "r")) == 0 ) {
			perror("/proc/mounts");
		}
		else {
			float gb=0;
			char c=' ';
			int offsetw = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth ("Filesystem      Size    Used       Available   Use% ");
			offsetw += 20;
			int i = 0;
			while ( ((mnt = getmntent(mountFile)) != 0) && (i<8 )) {
				if (::statfs(mnt->mnt_dir, &s) == 0) {
					switch (s.f_type)
					{
					case (int) 0xEF53:      /*EXT2 & EXT3*/
					case (int) 0x6969:      /*NFS*/
					case (int) 0xFF534D42:  /*CIFS*/
					case (int) 0x517B:      /*SMB*/
					case (int) 0x52654973:  /*REISERFS*/
					case (int) 0x65735546:  /*fuse for ntfs*/
					case (int) 0x58465342:  /*xfs*/
					case (int) 0x4d44:      /*msdos*/
						gb = 1024.0*1024.0;
						c = 'G';
						break;
					case (int) 0x72b6:	/*jffs2*/ 
					if (strcmp(mnt->mnt_fsname, "rootfs") == 0)
						continue;
						g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, "Filesystem      Size    Used       Available   Use% ", COL_MENUCONTENTINACTIVE);
						ypos+= mheight;

						gb = 1024.0;
						c = 'M';
					break;
					default:
						continue;
					}
					if ( s.f_blocks > 0 ) {
						long	blocks_used;
						long	blocks_percent_used;
						blocks_used = s.f_blocks - s.f_bfree;
						blocks_percent_used = (long)(blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);
						snprintf(ubuf,buf_size,
							 "%-10s  % 7.2f%c  % 7.2f%c  % 7.2f%c  % 5ld%%\n"
							 ,basename(mnt->mnt_fsname)
							 ,(s.f_blocks * (s.f_bsize / 1024.0)) / gb, c
							 ,((s.f_blocks - s.f_bfree)  * (s.f_bsize / 1024.0)) / gb, c
							 ,s.f_bavail * (s.f_bsize / 1024.0) / gb, c
							 ,blocks_percent_used
							);
						g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, ubuf, COL_MENUCONTENT);
						CProgressBar pb(pb_blink, -1, -1, 30, 100, 70, true);
						pb.paintProgressBarDefault(x+offsetw, ypos+3, 120, mheight-10, blocks_percent_used, 100);
						ypos+= mheight;
					}
					i++;
				}
			}
			endmntent(mountFile);
		}
	}
	if (sbuf)
		delete[] sbuf;
	if (ubuf)
		delete[] ubuf;

}

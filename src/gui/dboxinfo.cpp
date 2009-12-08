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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gui/dboxinfo.h>

#include <global.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>

#include <sys/sysinfo.h>
#include <sys/vfs.h>

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
	

    x=(((g_settings.screen_EndX- g_settings.screen_StartX)-width) / 2) + g_settings.screen_StartX;
	y=(((g_settings.screen_EndY- g_settings.screen_StartY)-height) / 2) + g_settings.screen_StartY;
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
	int i = 0;
	frameBuffer->paintBoxRel(x, ypos, width, hheight, COL_MENUHEAD_PLUS_0, ROUND_RADIUS, 1);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x+10, ypos+ hheight+1, width, "Uptime/Mem/Fs info", COL_MENUHEAD, 0, true); // UTF-8
	frameBuffer->paintBoxRel(x, ypos+ hheight, width, height- hheight, COL_MENUCONTENT_PLUS_0, ROUND_RADIUS, 2);

	ypos+= hheight + (mheight >>1);

#if 0
	int power = 0;
	int is_val = 0xdeadbabe;
	int fp_ver = 0;
	
	int fp = open("/dev/dbox/fp0", O_RDWR);
	if(fp > 0) {
		fp_ver = ioctl(fp, FP_IOCTL_GETID);
		if(fp_ver < 0) fp_ver = 0;
		ioctl(fp, FP_IOCTL_UPGRADE_CTRL, &is_val);
		if(is_val != 1) is_val = 0;
		ioctl(fp, FP_IOCTL_GET_LNB_CURRENT, &power);
		close(fp);
	} 
	else is_val = 0;
#endif
	char buf[256];
	FILE* fd = fopen("/proc/cpuinfo", "rt");
	if (fd==NULL) {
		printf("error while opening proc-cpuinfo\n" );
	} else {

		fgets(buf,255,fd);
		while(!feof(fd)) {
			if(fgets(buf,255,fd)!=NULL) {
				g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, buf, COL_MENUCONTENT);
				ypos+= mheight;
				i++;
				if(i > 6) break;
			}
		}
		fclose(fd);
	}

#if 0
	sprintf(buf, "FP version 1.0%d, Upgrade Available: %s, LNB current: %d\n", fp_ver, is_val == 1 ? "yes" : "no", power);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, buf, COL_MENUCONTENT);
	ypos+= mheight;
#endif

	int updays, uphours, upminutes;
	struct sysinfo info;
	struct tm *current_time;
	time_t current_secs;
	char ubuf[256], sbuf[256];

	memset(sbuf, 0, 256);
	time(&current_secs);
	current_time = localtime(&current_secs);

	sysinfo(&info);

	sprintf( ubuf, "%2d:%02d%s  up ", 
			current_time->tm_hour%12 ? current_time->tm_hour%12 : 12, 
			current_time->tm_min, current_time->tm_hour > 11 ? "pm" : "am");
	strcat(sbuf, ubuf);
	updays = (int) info.uptime / (60*60*24);
	if (updays) {
		sprintf(ubuf, "%d day%s, ", updays, (updays != 1) ? "s" : "");
		strcat(sbuf, ubuf);
	}
	upminutes = (int) info.uptime / 60;
	uphours = (upminutes / 60) % 24;
	upminutes %= 60;
	if(uphours)
		sprintf(ubuf,"%2d:%02d, ", uphours, upminutes);
	else
		sprintf(ubuf,"%d min, ", upminutes);
	strcat(sbuf, ubuf);

	sprintf(ubuf, "load: %ld.%02ld, %ld.%02ld, %ld.%02ld\n", 
			LOAD_INT(info.loads[0]), LOAD_FRAC(info.loads[0]), 
			LOAD_INT(info.loads[1]), LOAD_FRAC(info.loads[1]), 
			LOAD_INT(info.loads[2]), LOAD_FRAC(info.loads[2]));
	strcat(sbuf, ubuf);
	ypos+= mheight/2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, sbuf, COL_MENUCONTENT);
	ypos+= mheight;

	sprintf(ubuf, "memory total %dKb, free %dKb", (int) info.totalram/1024, (int) info.freeram/1024);
	ypos+= mheight/2;
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, ubuf, COL_MENUCONTENT);
	ypos+= mheight;

    struct statfs s;
    //long blocks_used;
    //long blocks_percent_used;

    if (::statfs("/var", &s) == 0) {
	//printf("/var filesystem total %ldKb, free %ldKb\n", (long)s.f_blocks*s.f_bsize/1024, (long)s.f_bfree*s.f_bsize/1024);
	sprintf(ubuf, "/var filesystem total %ldKb, free %ldKb", (long)s.f_blocks*s.f_bsize/1024, (long)s.f_bfree*s.f_bsize/1024);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, ubuf, COL_MENUCONTENT);
	ypos+= mheight;
    }
    else 
        perror("/var statfs");
    if (::statfs("/hdd", &s) == 0) {
//printf("STATFS: type %lX free: %ld\n", s.f_type, s.f_bfree);
	if( (s.f_type != (int) 0xEF53) && (s.f_type != (int) 0x52654973) && (s.f_type != (int) 0x6969) && (s.f_type != (int) 0xFF534D42) && (s.f_type != (int) 0x517B) && (s.f_type != (int) 0x58465342) )
		return;
	//printf("/hdd filesystem total %ldKb, free %ldKb\n", (long)(s.f_blocks/1024)*s.f_bsize, (long)(s.f_bfree/1024)*s.f_bsize);
	sprintf(ubuf, "/hdd filesystem total %ldKb, free %ldKb", (long)(s.f_blocks/1024)*s.f_bsize, (long)(s.f_bfree/1024)*s.f_bsize);
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->RenderString(x+ 10, ypos+ mheight, width, ubuf, COL_MENUCONTENT);
    }
}

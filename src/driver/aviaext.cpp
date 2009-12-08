/*
	LCD-Daemon  -   DBoxII-Project

	Copyright (C) 2004 Zwen@tuxbox.org

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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#if HAVE_DVB_API_VERSION >= 3
#include <dbox/aviaEXT.h>
#include <driver/aviaext.h>

#define AVIAEXT_DEV "/dev/dbox/aviaEXT"

CAViAExt* CAViAExt::getInstance()
{
	static CAViAExt* AViAExt = NULL;
	if(AViAExt == NULL)
	{
		AViAExt = new CAViAExt();
	}
	return AViAExt;
}

void CAViAExt::iecOn()
{
	int res,fd;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return;
	}
	res = ioctl(fd, AVIA_EXT_IEC_SET, 1);
	if (res<0)
		perror("aviaext: ioctl");
	close(fd);
}

void CAViAExt::iecOff()
{
	int res,fd;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return;
	}
	res = ioctl(fd, AVIA_EXT_IEC_SET, 0);
	if (res<0)
		perror("aviaext: ioctl");
	close(fd);
}

int CAViAExt::iecState()
{
	int res,fd;
	unsigned int param;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return -1;
	}
	res = ioctl(fd, AVIA_EXT_IEC_GET, &param);

	close(fd);

	if (res<0)
	{
		perror("aviaext: ioctl");
		return -1;
	}
	return param;
}	

void CAViAExt::playbackSPTS()
{
	int res,fd;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return;
	}
	res = ioctl(fd, AVIA_EXT_AVIA_PLAYBACK_MODE_SET, 1);
	if (res<0)
		perror("aviaext: ioctl");
	close(fd);
}

void CAViAExt::playbackPES()
{
	int res,fd;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return;
	}
	res = ioctl(fd, AVIA_EXT_AVIA_PLAYBACK_MODE_SET, 0);
	if (res<0)
		perror("aviaext: ioctl");
	close(fd);
}

int CAViAExt::playbackState()
{
	int res,fd;
	unsigned int param;
	
        if ((fd = open(AVIAEXT_DEV,O_RDWR))<0)
	{
		if (errno==ENOENT)
			fprintf (stderr,"%s does not exist, did you forget to load the aviaEXT module?\n",AVIAEXT_DEV);
		else
			perror ("aviaext: error opening /dev/dbox/aviaEXT");
		return -1;
	}
	res = ioctl(fd, AVIA_EXT_AVIA_PLAYBACK_MODE_GET, &param);

	close(fd);

	if (res<0)
	{
		perror("aviaext: ioctl");
		return -1;
	}
	return param;
}	
#endif

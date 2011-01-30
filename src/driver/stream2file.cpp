/*
 * $Id: stream2file.cpp,v 1.19 2005/01/12 20:40:22 chakazulu Exp $
 * 
 * streaming to file/disc
 * 
 * Copyright (C) 2004 Axel Buehning <diemade@tuxbox.org>,
 *                    thegoodguy <thegoodguy@berlios.de>
 *
 * based on code which is
 * Copyright (C) 2001 TripleDES
 * Copyright (C) 2000, 2001 Marcus Metzler <marcus@convergence.de>
 * Copyright (C) 2002 Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <stream2file.h>

#include <eventserver.h>
#include <neutrinoMessages.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <libgen.h>

#if HAVE_COOL_HARDWARE
#include <record_cs.h>
#include <driver/vfd.h>
#endif
#if HAVE_TRIPLEDRAGON
#include <record_td.h>
#include <driver/lcdd.h>
#endif
#include <ca_cs.h>
#include <zapit/cam.h>
#include <zapit/channel.h>

extern "C" {
#include <driver/genpsi.h>
}

static cRecord * record = NULL;
extern CZapitChannel *g_current_channel;
extern CCam *cam0;

extern bool autoshift;
extern bool autoshift_delete;

#define MAXPIDS		64
#define FILENAMEBUFFERSIZE 1024

static stream2file_status_t exit_flag = STREAM2FILE_STATUS_IDLE;

char rec_filename[FILENAMEBUFFERSIZE];

stream2file_error_msg_t start_recording(const char * const filename,
	const char * const info, const unsigned short vpid, const unsigned short * const pids,
	const unsigned int numpids)
{
	int fd;
	char buf[FILENAMEBUFFERSIZE];
	struct statfs s;

	if(autoshift)
		sprintf(rec_filename, "%s_temp", filename);
	else
		sprintf(rec_filename, "%s", filename);

	// write stream information (should wakeup the disk from standby, too)
	sprintf(buf, "%s.xml", rec_filename);

	char * dir = strdup(buf);
	int ret = statfs(dirname(dir), &s);
	free(dir);
	if((ret != 0) || (s.f_type == 0x72b6) || (s.f_type == 0x24051905)) {
		return STREAM2FILE_INVALID_DIRECTORY;
	}
	if ((fd = open(buf, O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		write(fd, info, strlen(info));
		fdatasync(fd);
		close(fd);
	} else {
		return STREAM2FILE_INVALID_DIRECTORY;
	}

	exit_flag = STREAM2FILE_STATUS_RUNNING;

	sprintf(buf, "%s.ts", rec_filename);

	printf("Record start: file %s vpid %x apid %x\n", buf, vpid, pids[0]);

	fd = open(buf, O_CREAT | O_RDWR | O_LARGEFILE | O_TRUNC , S_IRWXO | S_IRWXG | S_IRWXU);
	if(fd < 0) {
		perror(buf);
		return STREAM2FILE_INVALID_DIRECTORY;
	}
	genpsi(fd);

	if(!record)
		record = new cRecord(2);

	record->Open(numpids);

	if(!record->Start(fd, (unsigned short ) vpid, (unsigned short *) pids, numpids)) {
		record->Stop();
		delete record;
		record = NULL;
		return STREAM2FILE_INVALID_DIRECTORY;
	}
        if(g_current_channel) {
                cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() | DEMUX_DECODE_2 /*5*/, true); // demux 0 + 2, update

		int len;
		unsigned char * pmt = g_current_channel->getRawPmt(len);
		cCA * ca = cCA::GetInstance();
		ca->SendPMT(DEMUX_SOURCE_2, pmt, len);
	}

	CVFD::getInstance()->ShowIcon(VFD_ICON_CAM1, true);

	return STREAM2FILE_OK;
}

stream2file_error_msg_t stop_recording(const char * const info)
{
	char buf[FILENAMEBUFFERSIZE];
	char buf1[FILENAMEBUFFERSIZE];
	int fd;
	stream2file_error_msg_t ret = STREAM2FILE_RECORDING_THREADS_FAILED;

	//if(!autoshift || autoshift_delete) 
	{
		sprintf(buf, "%s.xml", rec_filename);
		if ((fd = open(buf, O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
			write(fd, info, strlen(info));
			fdatasync(fd);
			close(fd);
		} 
	}

	if(record) {
		record->Stop();
		delete record;
		record = NULL;
	}

	if (exit_flag == STREAM2FILE_STATUS_RUNNING) {
		exit_flag = STREAM2FILE_STATUS_IDLE;
		ret = STREAM2FILE_OK;
	}
	else
		ret = STREAM2FILE_RECORDING_THREADS_FAILED;

	if((autoshift && g_settings.auto_delete) || autoshift_delete) {
		sprintf(buf, "rm -f %s.ts &", rec_filename);
		sprintf(buf1, "%s.xml", rec_filename);
		autoshift_delete = false;
		system(buf);
		unlink(buf1);
	}
	CVFD::getInstance()->ShowIcon(VFD_ICON_CAM1, false);

	rec_filename[0] = 0;
	return ret;
}

stream2file_error_msg_t update_recording(const char * const info, const unsigned short vpid,
		const unsigned short * const pids, const unsigned int numpids)
{
//	stream2file_error_msg_t ret;
	char buf[FILENAMEBUFFERSIZE];
	int fd;

	sprintf(buf, "%s.xml", rec_filename);
	if ((fd = open(buf, O_SYNC | O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
		write(fd, info, strlen(info));
		fdatasync(fd);
		close(fd);
	}
	if(record)
		record->ChangePids((unsigned short) vpid, (unsigned short *) pids, numpids);

	if(g_current_channel)
		cam0->setCaPmt(g_current_channel->getCaPmt(), DEMUX_SOURCE_0, cam0->getCaMask() | DEMUX_DECODE_0 | DEMUX_DECODE_2, true);

	return STREAM2FILE_OK /*ret*/;
}

/*
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
 * (C) 2010-2011 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Suite 500 Boston, MA 02110-1335 USA
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <pthread.h>

#include <avs/avs_inf.h>
#include <clip/clipinfo.h>
#include "video_td.h"
#include <hardware/tddevices.h>
#define VIDEO_DEVICE "/dev/" DEVICE_NAME_VIDEO
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_VIDEO, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_VIDEO, this, args)

#define fop(cmd, args...) ({				\
	int _r;						\
	if (fd >= 0) { 					\
		if ((_r = ::cmd(fd, args)) < 0)		\
			lt_info(#cmd"(fd, "#args")\n");	\
		else					\
			lt_debug(#cmd"(fd, "#args")\n");\
	}						\
	else { _r = fd; } 				\
	_r;						\
})

cVideo * videoDecoder = NULL;
int system_rev = 0;

#if 0
/* this would be necessary for the DirectFB implementation of ShowPicture */
#include <directfb.h>
#include <tdgfx/stb04gfx.h>
extern IDirectFB *dfb;
extern IDirectFBSurface *dfbdest;
#endif

extern struct Ssettings settings;
static pthread_mutex_t stillp_mutex = PTHREAD_MUTEX_INITIALIZER;

/* debugging hacks */
static bool noscart = false;

cVideo::cVideo(int, void *, void *)
{
	lt_debug("%s\n", __FUNCTION__);
	if ((fd = open(VIDEO_DEVICE, O_RDWR)) < 0)
		lt_info("%s cannot open %s: %m\n", __FUNCTION__, VIDEO_DEVICE);
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	playstate = VIDEO_STOPPED;
	croppingMode = VID_DISPMODE_NORM;
	outputformat = VID_OUTFMT_RGBC_SVIDEO;
	scartvoltage = -1;
	z[0] = 100;
	z[1] = 100;
	zoomvalue = &z[0];
	const char *blanknames[2] = { "/share/tuxbox/blank_576.mpg", "/share/tuxbox/blank_480.mpg" };
	int blankfd;
	struct stat st;

	for (int i = 0; i < 2; i++)
	{
		blank_data[i] = NULL; /* initialize */
		blank_size[i] = 0;
		blankfd = open(blanknames[i], O_RDONLY);
		if (blankfd < 0)
		{
			lt_info("%s cannot open %s: %m", __FUNCTION__, blanknames[i]);
			continue;
		}
		if (fstat(blankfd, &st) != -1 && st.st_size > 0)
		{
			blank_size[i] = st.st_size;
			blank_data[i] = malloc(blank_size[i]);
			if (! blank_data[i])
				lt_info("%s malloc failed (%m)\n", __FUNCTION__);
			else if (read(blankfd, blank_data[i], blank_size[i]) != blank_size[i])
			{
				lt_info("%s short read (%m)\n", __FUNCTION__);
				free(blank_data[i]); /* don't leak... */
				blank_data[i] = NULL;
			}
		}
		close(blankfd);
	}
	video_standby = 0;
	noscart = (getenv("TRIPLE_NOSCART") != NULL);
	if (noscart)
		lt_info("%s TRIPLE_NOSCART variable prevents SCART switching\n", __FUNCTION__);
}

cVideo::~cVideo(void)
{
	playstate = VIDEO_STOPPED;
	for (int i = 0; i < 2; i++)
	{
		if (blank_data[i])
			free(blank_data[i]);
		blank_data[i] = NULL;
	}
	/* disable DACs and SCART voltage */
	Standby(true);
	if (fd >= 0)
		close(fd);
}

int cVideo::setAspectRatio(int aspect, int mode)
{
	static int _mode = -1;
	static int _aspect = -1;
	vidDispSize_t dsize = VID_DISPSIZE_UNKNOWN;
	vidDispMode_t dmode = VID_DISPMODE_NORM;
	/* 1 = 4:3, 3 = 16:9, 4 = 2.21:1, 0 = unknown */
	int v_ar = getAspectRatio();

	if (aspect != -1)
		_aspect = aspect;
	if (mode != -1)
		_mode = mode;
	lt_info("%s(%d, %d)_(%d, %d) v_ar %d\n", __FUNCTION__, aspect, mode, _aspect, _mode, v_ar);

	/* values are hardcoded in neutrino_menue.cpp, "2" is 14:9 -> not used */
	if (_aspect != -1)
	{
		switch(_aspect)
		{
		case 1:
			dsize = VID_DISPSIZE_4x3;
			scartvoltage = 12;
			break;
		case 3:
			dsize = VID_DISPSIZE_16x9;
			scartvoltage = 6;
			break;
		default:
			break;
		}
	}
	if (_mode != -1)
	{
		int zoom = 100 * 16 / 14; /* 16:9 vs 14:9 */
		switch(_mode)
		{
		case DISPLAY_AR_MODE_NONE:
			if (v_ar < 3)
				dsize = VID_DISPSIZE_4x3;
			else
				dsize = VID_DISPSIZE_16x9;
			break;
		case DISPLAY_AR_MODE_LETTERBOX:
			dmode = VID_DISPMODE_LETTERBOX;
			break;
		case DISPLAY_AR_MODE_PANSCAN:
			zoom = 100 * 5 / 4;
		case DISPLAY_AR_MODE_PANSCAN2:
			if ((v_ar < 3 && _aspect == 3) || (v_ar >= 3 && _aspect == 1))
			{
				/* unfortunately, this partly reimplements the setZoom code... */
				dsize = VID_DISPSIZE_UNKNOWN;
				dmode = VID_DISPMODE_SCALE;
				SCALEINFO s;
				memset(&s, 0, sizeof(s));
				if (v_ar < 3) { /* 4:3 */
					s.src.hori_size = 720;
					s.src.vert_size = 2 * 576 - 576 * zoom / 100;
					s.des.hori_size = zoom * 720 * 3/4 / 100;
					s.des.vert_size = 576;
				} else {
					s.src.hori_size = 2 * 720 - 720 * zoom / 100;
					s.src.vert_size = 576;
					s.des.hori_size = 720;
					s.des.vert_size = zoom * 576 * 3/4 / 100;
				}
				s.des.vert_off = (576 - s.des.vert_size) / 2;
				s.des.hori_off = (720 - s.des.hori_size) / 2;
				lt_debug("PANSCAN2: %d%% src: %d:%d:%d:%d dst: %d:%d:%d:%d\n", zoom,
					s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
					s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
				fop(ioctl, MPEG_VID_SCALE_ON);
				fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
			}
		default:
			break;
		}
		if (dmode != VID_DISPMODE_SCALE)
			fop(ioctl, MPEG_VID_SCALE_OFF);
		setCroppingMode(dmode);
	}
	const char *ds[] = { "4x3", "16x9", "2.21", "unknown" };
	const char *d;
	if (dsize >=0 && dsize < 4)
		d = ds[dsize];
	else
		d = "invalid!";
	lt_debug("%s dispsize(%d) (%s)\n", __FUNCTION__, dsize, d);
	fop(ioctl, MPEG_VID_SET_DISPSIZE, dsize);

	int avsfd = open("/dev/stb/tdsystem", O_RDONLY);
	if (avsfd < 0)
	{
		perror("open tdsystem");
		return 0;
	}
	if (!noscart && scartvoltage > 0 && video_standby == 0)
	{
		lt_info("%s set SCART_PIN_8 to %dV\n", __FUNCTION__, scartvoltage);
		if (ioctl(avsfd, IOC_AVS_SCART_PIN8_SET, scartvoltage) < 0)
			perror("IOC_AVS_SCART_PIN8_SET");
	}
	close(avsfd);
	return 0;
}

int cVideo::getAspectRatio(void)
{
	VIDEOINFO v;
	/* this memset silences *TONS* of valgrind warnings */
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);
	if (v.pel_aspect_ratio < VID_DISPSIZE_4x3 || v.pel_aspect_ratio > VID_DISPSIZE_UNKNOWN)
	{
		lt_info("%s invalid value %d, returning 0/unknown fd: %d", __FUNCTION__, v.pel_aspect_ratio, fd);
		return 0;
	}
	/* convert to Coolstream api values. Taken from streaminfo2.cpp */
	switch (v.pel_aspect_ratio)
	{
		case VID_DISPSIZE_4x3:
			return 1;
		case VID_DISPSIZE_16x9:
			return 3;
		case VID_DISPSIZE_221x100:
			return 4;
		default:
			return 0;
	}
}

int cVideo::setCroppingMode(vidDispMode_t format)
{
	croppingMode = format;
	const char *format_string[] = { "norm", "letterbox", "unknown", "mode_1_2", "mode_1_4", "mode_2x", "scale", "disexp" };
	const char *f;
	if (format >= VID_DISPMODE_NORM && format <= VID_DISPMODE_DISEXP)
		f = format_string[format];
	else
		f = "ILLEGAL format!";
	lt_debug("%s(%d) => %s\n", __FUNCTION__, format, f);
	return fop(ioctl, MPEG_VID_SET_DISPMODE, format);
}

int cVideo::Start(void * /*PcrChannel*/, unsigned short /*PcrPid*/, unsigned short /*VideoPid*/, void * /*hChannel*/)
{
	lt_debug("%s playstate=%d\n", __FUNCTION__, playstate);
	if (playstate == VIDEO_PLAYING)
		return 0;
	if (playstate == VIDEO_FREEZED)  /* in theory better, but not in practice :-) */
		fop(ioctl, MPEG_VID_CONTINUE);
	playstate = VIDEO_PLAYING;
	fop(ioctl, MPEG_VID_PLAY);
	return fop(ioctl, MPEG_VID_SYNC_ON, VID_SYNC_AUD);
}

int cVideo::Stop(bool blank)
{
	lt_debug("%s(%d)\n", __FUNCTION__, blank);
	if (blank)
	{
		playstate = VIDEO_STOPPED;
		fop(ioctl, MPEG_VID_STOP);
		return setBlank(1);
	}
	playstate = VIDEO_FREEZED;
	return fop(ioctl, MPEG_VID_FREEZE);
}

int cVideo::setBlank(int)
{
	lt_debug("%s\n", __FUNCTION__);
	/* The TripleDragon has no VIDEO_SET_BLANK ioctl.
	   instead, you write a black still-MPEG Iframe into the decoder.
	   The original software uses different files for 4:3 and 16:9 and
	   for PAL and NTSC. I optimized that a little bit
	 */
	int index = 0; /* default PAL */
	int ret = 0;
	VIDEOINFO v;
	BUFINFO buf;
	pthread_mutex_lock(&stillp_mutex);
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);

	if ((v.v_size % 240) == 0) /* NTSC */
	{
		lt_info("%s NTSC format detected", __FUNCTION__);
		index = 1;
	}

	if (blank_data[index] == NULL) /* no MPEG found */
	{
		ret = -1;
		goto out;
	}
	/* hack: this might work only on those two still-MPEG files!
	   I diff'ed the 4:3 and the 16:9 still mpeg from the original
	   soft and spotted the single bit difference, so there is no
	   need to keep two different MPEGs in memory
	   If we would read them from disk all the time it would be
	   slower and it might wake up the drive occasionally */
	if (v.pel_aspect_ratio == VID_DISPSIZE_4x3)
		((char *)blank_data[index])[7] &= ~0x10; // clear the bit
	else
		((char *)blank_data[index])[7] |=  0x10; // set the bit

	//WARN("blank[7] == 0x%02x", ((char *)blank_data[index])[7]);

	buf.ulLen = blank_size[index];
	buf.ulStartAdrOff = (int)blank_data[index];
	fop(ioctl, MPEG_VID_STILLP_WRITE, &buf);
	ret = fop(ioctl, MPEG_VID_SELECT_SOURCE, VID_SOURCE_DEMUX);
 out:
	pthread_mutex_unlock(&stillp_mutex);
	return ret;
}

int cVideo::SetVideoSystem(int video_system, bool remember)
{
	lt_info("%s(%d, %d)\n", __FUNCTION__, video_system, remember);
	if (video_system > VID_DISPFMT_SECAM || video_system < 0)
		video_system = VID_DISPFMT_PAL;
        return fop(ioctl, MPEG_VID_SET_DISPFMT, video_system);
}

int cVideo::getPlayState(void)
{
	return playstate;
}

void cVideo::SetVideoMode(analog_mode_t mode)
{
	lt_debug("%s(%d)\n", __FUNCTION__, mode);
	switch(mode)
	{
		case ANALOG_SD_YPRPB_SCART:
			outputformat = VID_OUTFMT_YBR_SVIDEO;
			break;
		case ANALOG_SD_RGB_SCART:
			outputformat = VID_OUTFMT_RGBC_SVIDEO;
			break;
		default:
			lt_info("%s unknown mode %d\n", __FUNCTION__, mode);
			return;
	}
	fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
}

void cVideo::ShowPicture(const char * fname)
{
	lt_debug("%s(%s)\n", __FUNCTION__, fname);
	char destname[512];
	char cmd[512];
	char *p;
	void *data;
	int mfd;
	struct stat st;
	strcpy(destname, "/var/cache");
	mkdir(destname, 0755);
	/* the cache filename is (example for /share/tuxbox/neutrino/icons/radiomode.jpg):
	   /var/cache/share.tuxbox.neutrino.icons.radiomode.jpg.m2v
	   build that filename first...
	   TODO: this could cause name clashes, use a hashing function instead... */
	strcat(destname, fname);
	p = &destname[strlen("/var/cache/")];
	while ((p = strchr(p, '/')) != NULL)
		*p = '.';
	strcat(destname, ".m2v");
	/* ...then check if it exists already...
	   TODO: check if the cache file is older than the jpeg file... */
	if (access(destname, R_OK))
	{
		/* it does not exist, so call ffmpeg to create it... */
		sprintf(cmd, "ffmpeg -y -f mjpeg -i '%s' -s 704x576 '%s' </dev/null",
							fname, destname);
		system(cmd); /* TODO: use libavcodec to directly convert it */
	}
	/* the mutex is a workaround: setBlank is apparently called from
	   a differnt thread and takes slightly longer, so that the decoder
	   was blanked immediately after displaying the image, which is not
	   what we want. the mutex ensures proper ordering. */
	pthread_mutex_lock(&stillp_mutex);
	mfd = open(destname, O_RDONLY);
	if (mfd < 0)
	{
		lt_info("%s cannot open %s: %m", __FUNCTION__, destname);
		goto out;
	}
	if (fstat(mfd, &st) != -1 && st.st_size > 0)
	{
		data = malloc(st.st_size);
		if (! data)
			lt_info("%s malloc failed (%m)\n", __FUNCTION__);
		else if (read(mfd, data, st.st_size) != st.st_size)
			lt_info("%s short read (%m)\n", __FUNCTION__);
		else
		{
			BUFINFO buf;
			buf.ulLen = st.st_size;
			buf.ulStartAdrOff = (int)data;
			Stop(false);
			fop(ioctl, MPEG_VID_STILLP_WRITE, &buf);
		}
		free(data);
	}
	close(mfd);
 out:
	pthread_mutex_unlock(&stillp_mutex);
	return;
#if 0
	/* DirectFB based picviewer: works, but is slow and the infobar
	   draws in the same plane */
	int width;
	int height;
	if (!fname)
		return;

	IDirectFBImageProvider *provider;
	DFBResult err = dfb->CreateImageProvider(dfb, fname, &provider);
	if (err)
	{
		fprintf(stderr, "cVideo::ShowPicture: CreateImageProvider error!\n");
		return;
	}

	DFBSurfaceDescription desc;
	provider->GetSurfaceDescription (provider, &desc);
	width = desc.width;
	height = desc.height;
	provider->RenderTo(provider, dfbdest, NULL);
	provider->Release(provider);
#endif
}

void cVideo::StopPicture()
{
	lt_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_SELECT_SOURCE, VID_SOURCE_DEMUX);
}

void cVideo::Standby(unsigned int bOn)
{
	lt_debug("%s(%d)\n", __FUNCTION__, bOn);
	if (bOn)
	{
		setBlank(1);
		fop(ioctl, MPEG_VID_SET_OUTFMT, VID_OUTFMT_DISABLE_DACS);
	} else
		fop(ioctl, MPEG_VID_SET_OUTFMT, outputformat);
	routeVideo(bOn);
	video_standby = bOn;
}

int cVideo::getBlank(void)
{
	lt_debug("%s\n", __FUNCTION__);
	return 0;
}

/* set zoom in percent (100% == 1:1) */
int cVideo::setZoom(int zoom)
{
	if (zoom == -1) // "auto" reset
		zoom = *zoomvalue;

	if (zoom > 150 || zoom < 100)
		return -1;

	*zoomvalue = zoom;

	if (zoom == 100)
	{
		setCroppingMode(croppingMode);
		return fop(ioctl, MPEG_VID_SCALE_OFF);
	}

	/* the SCALEINFO describes the source and destination of the scaled
	   video. "src" is the part of the source picture that gets scaled,
	   "dst" is the area on the screen where this part is displayed
	   Messing around with MPEG_VID_SET_SCALE_POS disables the automatic
	   letterboxing, which, as I guess, is only a special case of
	   MPEG_VID_SET_SCALE_POS. Therefor we need to care for letterboxing
	   etc here, which is probably not yet totally correct */
	SCALEINFO s;
	memset(&s, 0, sizeof(s));
	if (zoom > 100)
	{
		/* 1 = 4:3, 3 = 16:9, 4 = 2.21:1, 0 = unknown */
		int x = getAspectRatio();
		if (x < 3 && croppingMode == VID_DISPMODE_NORM)
		{
			s.src.hori_size = 720;
			s.des.hori_size = 720 * 3/4 * zoom / 100;
			if (s.des.hori_size > 720)
			{
				/* the destination exceeds the screen size.
				   TODO: decrease source size to allow higher
				   zoom factors (is this useful ?) */
				s.des.hori_size = 720;
				zoom = 133; // (720*4*100)/(720*3)
				*zoomvalue = zoom;
			}
		}
		else
		{
			s.src.hori_size = 2 * 720 - 720 * zoom / 100;
			s.des.hori_size = 720;
		}
		s.src.vert_size = 2 * 576 - 576 * zoom / 100;
		s.des.hori_off = (720 - s.des.hori_size) / 2;
		s.des.vert_size = 576;
	}
/* not working correctly (wrong formula) and does not make sense IMHO
	else
	{
		s.src.hori_size = 720;
		s.src.vert_size = 576;
		s.des.hori_size = 720 * zoom / 100;
		s.des.vert_size = 576 * zoom / 100;
		s.des.hori_off = (720 - s.des.hori_size) / 2;
		s.des.vert_off = (576 - s.des.vert_size) / 2;
	}
 */
	lt_debug("%s %d%% src: %d:%d:%d:%d dst: %d:%d:%d:%d\n", __FUNCTION__, zoom,
		s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
		s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
	fop(ioctl, MPEG_VID_SET_DISPMODE, VID_DISPMODE_SCALE);
	fop(ioctl, MPEG_VID_SCALE_ON);
	return fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
}

#if 0
int cVideo::getZoom(void)
{
	return *zoomvalue;
}

void cVideo::setZoomAspect(int index)
{
	if (index < 0 || index > 1)
		WARN("index out of range");
	else
		zoomvalue = &z[index];
}
#endif

/* this function is regularly called, checks if video parameters
   changed and triggers appropriate actions */
void cVideo::VideoParamWatchdog(void)
{
	static unsigned int _v_info = (unsigned int) -1;
	unsigned int v_info;
	if (fd == -1)
		return;
	ioctl(fd, MPEG_VID_GET_V_INFO_RAW, &v_info);
	if (_v_info != v_info)
	{
		lt_debug("%s params changed. old: %08x new: %08x\n", __FUNCTION__, _v_info, v_info);
		setAspectRatio(-1, -1);
	}
	_v_info = v_info;
}

void cVideo::Pig(int x, int y, int w, int h, int /*osd_w*/, int /*osd_h*/)
{
	/* x = y = w = h = -1 -> reset / "hide" PIG */
	if (x == -1 && y == -1 && w == -1 && h == -1)
	{
		setZoom(-1);
		setAspectRatio(-1, -1);
		return;
	}
	SCALEINFO s;
	memset(&s, 0, sizeof(s));
	s.src.hori_size = 720;
	s.src.vert_size = 576;
	s.des.hori_off = x;
	s.des.vert_off = y;
	s.des.hori_size = w;
	s.des.vert_size = h;
	lt_debug("%s src: %d:%d:%d:%d dst: %d:%d:%d:%d", __FUNCTION__,
		s.src.hori_off,s.src.vert_off,s.src.hori_size,s.src.vert_size,
		s.des.hori_off,s.des.vert_off,s.des.hori_size,s.des.vert_size);
	fop(ioctl, MPEG_VID_SET_DISPMODE, VID_DISPMODE_SCALE);
	fop(ioctl, MPEG_VID_SCALE_ON);
	fop(ioctl, MPEG_VID_SET_SCALE_POS, &s);
}

void cVideo::getPictureInfo(int &width, int &height, int &rate)
{
	VIDEOINFO v;
	/* this memset silences *TONS* of valgrind warnings */
	memset(&v, 0, sizeof(v));
	ioctl(fd, MPEG_VID_GET_V_INFO, &v);
	/* convert to Coolstream API */
	rate = (int)v.frame_rate - 1;
	width = (int)v.h_size;
	height = (int)v.v_size;
}

void cVideo::SetSyncMode(AVSYNC_TYPE Mode)
{
	lt_debug("%s %d\n", __FUNCTION__, Mode);
	/*
	 * { 0, LOCALE_OPTIONS_OFF },
	 * { 1, LOCALE_OPTIONS_ON  },
	 * { 2, LOCALE_AUDIOMENU_AVSYNC_AM }
	 */
	switch(Mode)
	{
		case 0:
			ioctl(fd, MPEG_VID_SYNC_OFF);
			break;
		case 1:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_VID);
			break;
		default:
			ioctl(fd, MPEG_VID_SYNC_ON, VID_SYNC_AUD);
			break;
	}
};

int cVideo::SetStreamType(VIDEO_FORMAT type)
{
	static const char *VF[] = {
		"VIDEO_FORMAT_MPEG2",
		"VIDEO_FORMAT_MPEG4",
		"VIDEO_FORMAT_VC1",
		"VIDEO_FORMAT_JPEG",
		"VIDEO_FORMAT_GIF",
		"VIDEO_FORMAT_PNG"
	};

	lt_debug("%s type=%s\n", __FUNCTION__, VF[type]);
	return 0;
}

void cVideo::routeVideo(int standby)
{
	lt_debug("%s(%d)\n", __FUNCTION__, standby);

	int avsfd = open("/dev/stb/tdsystem", O_RDONLY);
	if (avsfd < 0)
	{
		perror("open tdsystem");
		return;
	}

	/* in standby, we always route VCR scart to the TV. Once there is a UI
	   to configure this, we can think more about this... */
	if (standby)
	{
		lt_info("%s set fastblank and pin8 to follow VCR SCART, route VCR to TV\n", __FUNCTION__);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, (unsigned char)3) < 0)
			perror("IOC_AVS_FASTBLANK_SET, 3");
		/* TODO: should probably depend on aspect ratio setting */
		if (ioctl(avsfd, IOC_AVS_SCART_PIN8_FOLLOW_VCR) < 0)
			perror("IOC_AVS_SCART_PIN8_FOLLOW_VCR");
		if (ioctl(avsfd, IOC_AVS_ROUTE_VCR2TV) < 0)
			perror("IOC_AVS_ROUTE_VCR2TV");
	} else {
		unsigned char fblk = 1;
		lt_info("%s set fastblank=%d pin8=%dV, route encoder to TV\n", __FUNCTION__, fblk, scartvoltage);
		if (ioctl(avsfd, IOC_AVS_FASTBLANK_SET, fblk) < 0)
			perror("IOC_AVS_FASTBLANK_SET, fblk");
		if (!noscart && ioctl(avsfd, IOC_AVS_SCART_PIN8_SET, scartvoltage) < 0)
			perror("IOC_AVS_SCART_PIN8_SET");
		if (ioctl(avsfd, IOC_AVS_ROUTE_ENC2TV) < 0)
			perror("IOC_AVS_ROUTE_ENC2TV");
	}
	close(avsfd);
}

void cVideo::FastForwardMode(int mode)
{
	lt_debug("%s\n", __FUNCTION__);
	fop(ioctl, MPEG_VID_FASTFORWARD, mode);
}

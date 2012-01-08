/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2011 CoolStream International Ltd

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <png.h>

#include <global.h>
#include <neutrino.h>
#include <gui/filebrowser.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <daemonc/remotecontrol.h>
#include <zapit/debug.h>

#include <video.h>
#include <cs_api.h>
#include <driver/screenshot.h>

extern "C" {
#include <jpeglib.h>
}

extern cVideo *videoDecoder;

/* constructor, defaults is empty fname and CScreenShot::FORMAT_JPG format */
CScreenShot::CScreenShot(const std::string fname, screenshot_format_t fmt)
{
	format = fmt;
	filename = fname;
	pixel_data = NULL;
	fd = NULL;
}

CScreenShot::~CScreenShot()
{
}

/* try to get video frame data in ARGB format, restore GXA state */
bool CScreenShot::GetData()
{
	static OpenThreads::Mutex mutex;
	bool res = false;

	mutex.lock();

#if 0 // to enable after libcs/drivers update
	res = videoDecoder->GetScreenImage(pixel_data, xres, yres);
#endif

#ifdef USE_NEVIS_GXA
	/* sort of hack. GXA used to transfer/convert live image to RGB,
	 * so setup GXA back */
	CFrameBuffer::getInstance()->setupGXA();
#endif
	mutex.unlock();
	if (!res) {
		printf("CScreenShot::Start: GetScreenImage failed\n");
		return false;
	}

	printf("CScreenShot::GetData: data: %x %d x %d\n", (int) pixel_data, xres, yres);
	return true;
}

/* start ::run in new thread to save file in selected format */
bool CScreenShot::Start()
{
	bool ret = false;
	if(GetData())
		ret = (start() == 0);
	return ret;
}

/* thread function to save data asynchroniosly. delete itself after saving */
void CScreenShot::run()
{
	printf("CScreenShot::run save to %s format %d\n", filename.c_str(), format);
	detach();
	setCancelModeDisable();
	setSchedulePriority(THREAD_PRIORITY_MIN);
	bool ret = SaveFile();
	printf("CScreenShot::run: %s finished: %d\n", filename.c_str(), ret);
	delete this;
}

/* save file in sync mode, return true if save ok, or false */
bool CScreenShot::StartSync()
{
	bool ret = false;
	printf("CScreenShot::StartSync save to %s format %d\n", filename.c_str(), format);
	if(GetData())
		ret = SaveFile();

	printf("CScreenShot::StartSync: %s finished: %d\n", filename.c_str(), ret);
	return ret;
}

/* save file in selected format, free data received from video decoder */
bool CScreenShot::SaveFile()
{
	bool ret = true;
	switch (format) {
	case FORMAT_PNG:
		ret = SavePng();
		break;
	default:
		printf("CScreenShot::SaveFile unsupported format %d, using jpeg\n", format);
	case FORMAT_JPG:
		ret = SaveJpg();
		break;
	}
	cs_free_uncached((void *) pixel_data);
	return ret;
}

/* try to open file, return true if success, or false */
bool CScreenShot::OpenFile()
{
	fd = fopen(filename.c_str(), "w");
	if (!fd) {
		printf("CScreenShot::OpenFile: failed to open %s\n", filename.c_str());
		return false;
	}
	return true;
}

/* save screenshot in png format, return true if success, or false */
bool CScreenShot::SavePng()
{
	png_bytep *row_pointers;
	png_structp png_ptr;
	png_infop info_ptr;

	TIMER_START();
	if(!OpenFile())
		return false;

	row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * yres);
	if (!row_pointers) {
		printf("CScreenShot::SavePng: malloc error\n");
		fclose(fd);
		return false;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)NULL, (png_error_ptr)NULL, (png_error_ptr)NULL);
	info_ptr = png_create_info_struct(png_ptr);
#if (PNG_LIBPNG_VER < 10500)
	if (setjmp(png_ptr->jmpbuf))
#else
	if (setjmp(png_jmpbuf(png_ptr)))
#endif
	{
		printf("CScreenShot::SavePng: %s save error\n", filename.c_str());
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fd);
		return false;
	}

	png_init_io(png_ptr, fd);

	int y;
	for (y=0; y<yres; y++) {
		row_pointers[y] = pixel_data + (y*xres*4);
	}

	png_set_IHDR(png_ptr, info_ptr, xres, yres, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	//png_set_filter (png_ptr, 0, PNG_FILTER_NONE);

	png_set_compression_level(png_ptr, Z_BEST_SPEED);

	png_set_bgr(png_ptr);
	png_set_invert_alpha(png_ptr);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row_pointers);
	fclose(fd);
	TIMER_STOP(filename.c_str());
	return true;
}

#define SWAP(x,y)       { x ^= y; y ^= x; x ^= y; }

/* from libjpg example.c */
struct my_error_mgr {
	struct jpeg_error_mgr pub;    /* "public" fields */
	jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	/* Always display the message. */
	/* We could postpone this until after returning, if we chose. */
	(*cinfo->err->output_message) (cinfo);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}

/* save screenshot in jpg format, return true if success, or false */
bool CScreenShot::SaveJpg()
{
	int quality = 90;

	if(!OpenFile())
		return false;

	for (int y = 0; y < yres; y++) {
		int xres1 = y*xres*3;
		int xres2 = xres1+2;
		for (int x = 0; x < xres; x++) {
			int x2 = x*3;
			memcpy(pixel_data + x2 + xres1, pixel_data + x*4 + y*xres*4, 3);
			SWAP(pixel_data[x2 + xres1], pixel_data[x2 + xres2]);
		}
	}

	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jerr;
	JSAMPROW row_pointer[1];
	unsigned int row_stride;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;

	if (setjmp(jerr.setjmp_buffer)) {
		printf("CScreenShot::SaveJpg: %s save error\n", filename.c_str());
		jpeg_destroy_compress(&cinfo);
		fclose(fd);
		return false;
	}

	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, fd);

	cinfo.image_width = xres;
	cinfo.image_height = yres;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	cinfo.dct_method = JDCT_IFAST;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	row_stride = xres * 3;

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = & pixel_data[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(fd);
	return true;
}

bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);
bool sectionsd_getEPGidShort(event_id_t epgID, CShortEPGData * epgdata);

/* 
 * create filename member from channel name and its current EPG data,
 * with added date and time including msecs and suffix for selected format 
 */
void CScreenShot::MakeFileName(const t_channel_id channel_id)
{
	char		fname[512]; // UTF-8
	std::string	channel_name;
	CEPGData	epgData;
	unsigned int	pos = 0;

	//TODO settings to select screenshot dir ?
	sprintf(fname, "%s/", g_settings.network_nfs_recordingdir);

	pos = strlen(fname);

	channel_name = g_Zapit->getChannelName(channel_id);
	if (!(channel_name.empty())) {
		strcpy(&(fname[pos]), UTF8_TO_FILESYSTEM_ENCODING(channel_name.c_str()));
		ZapitTools::replace_char(&fname[pos]);
		strcat(fname, "_");
	}
	pos = strlen(fname);

	if(sectionsd_getActualEPGServiceKey(channel_id&0xFFFFFFFFFFFFULL, &epgData)) {
		CShortEPGData epgdata;
		if(sectionsd_getEPGidShort(epgData.eventID, &epgdata)) {
			if (!(epgdata.title.empty())) {
				strcpy(&(fname[pos]), epgdata.title.c_str());
				ZapitTools::replace_char(&fname[pos]);
			}
		}
	}
	pos = strlen(fname);

	struct timeval tv;
	gettimeofday(&tv, NULL);	
	strftime(&(fname[pos]), sizeof(fname) - pos - 1, "_%Y%m%d_%H%M%S", localtime(&tv.tv_sec));
	pos = strlen(fname);
	snprintf(&(fname[pos]), sizeof(fname) - pos - 1, "_%d", (int) tv.tv_usec/1000);

	switch (format) {
	case FORMAT_PNG:
		strcat(fname, ".png");
		break;
	default:
		printf("CScreenShot::MakeFileName unsupported format %d, using jpeg\n", format);
	case FORMAT_JPG:
		strcat(fname, ".jpg");
		break;
	}
	printf("CScreenShot::MakeFileName: [%s]\n", fname);
	filename = std::string(fname);
}

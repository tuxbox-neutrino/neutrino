/*
 * neutrino specific framebuffer stuff for yaft framebuffer terminal
 * (C) 2018 Stefan Seyfried
 * License: GPL-2.0
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * derived from yaft/fb/linux.h,
 * original code
 * Copyright (c) 2012 haru <uobikiemukot at gmail dot com>
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 */
#include <driver/framebuffer.h>

enum {
	CMAP_COLOR_LENGTH = sizeof(__u16) * BITS_PER_BYTE,
};

/* initialize struct fb_info_t */
void set_bitfield(struct fb_var_screeninfo *vinfo,
	struct bitfield_t *red, struct bitfield_t *green, struct bitfield_t *blue)
{
	red->length   = vinfo->red.length;
	green->length = vinfo->green.length;
	blue->length  = vinfo->blue.length;

	red->offset   = vinfo->red.offset;
	green->offset = vinfo->green.offset;
	blue->offset  = vinfo->blue.offset;
}

enum fb_type_t set_type(__u32 type)
{
	if (type == FB_TYPE_PACKED_PIXELS)
		return YAFT_FB_TYPE_PACKED_PIXELS;
	else if (type == FB_TYPE_PLANES)
		return YAFT_FB_TYPE_PLANES;
	else
		return YAFT_FB_TYPE_UNKNOWN;
}

enum fb_visual_t set_visual(__u32 visual)
{
	if (visual == FB_VISUAL_TRUECOLOR)
		return YAFT_FB_VISUAL_TRUECOLOR;
	else if (visual == FB_VISUAL_DIRECTCOLOR)
		return YAFT_FB_VISUAL_DIRECTCOLOR;
	else if (visual == FB_VISUAL_PSEUDOCOLOR)
		return YAFT_FB_VISUAL_PSEUDOCOLOR;
	else
		return YAFT_FB_VISUAL_UNKNOWN;
}

bool set_fbinfo(int /*fd*/, struct fb_info_t *info)
{
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
#if 0
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo)) {
		logging(ERROR, "ioctl: FBIOGET_FSCREENINFO failed\n");
		return false;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		logging(ERROR, "ioctl: FBIOGET_VSCREENINFO failed\n");
		return false;
	}
#endif
	struct fb_var_screeninfo *var = info->cfb->getScreenInfo();
	memcpy(&vinfo, var, sizeof(struct fb_var_screeninfo));
	memset(&finfo, 0, sizeof(struct fb_fix_screeninfo));
	finfo.line_length = info->cfb->getScreenX(); // * (vinfo.bits_per_pixel / 8);
	finfo.visual = FB_VISUAL_TRUECOLOR;
	finfo.type   = FB_TYPE_PACKED_PIXELS;
	strcpy(finfo.id, "neutrino FB");

	set_bitfield(&vinfo, &info->red, &info->green, &info->blue);

	info->bits_per_pixel  = vinfo.bits_per_pixel;
	info->bytes_per_pixel = my_ceil(info->bits_per_pixel, BITS_PER_BYTE);
	info->width  = info->cfb->getScreenWidth();  //vinfo.xres;
	info->height = info->cfb->getScreenHeight(); //vinfo.yres;
	info->xstart = info->cfb->getScreenX();
	info->ystart = info->cfb->getScreenY();
	info->line_length = info->width * info->bytes_per_pixel;
	info->screen_size = info->line_length * info->height; //vinfo.yres;

	info->type   = set_type(finfo.type);
	info->visual = set_visual(finfo.visual);

	/* check screen [xy]offset and initialize because linux console changes these values */
	if (vinfo.xoffset != 0 || vinfo.yoffset != 0) {
		vinfo.xoffset = vinfo.yoffset = 0;
#if 0
		if (ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo))
			logging(WARN, "couldn't reset offset (x:%d y:%d)\n", vinfo.xoffset, vinfo.yoffset);
#endif
	}

	return true;
}

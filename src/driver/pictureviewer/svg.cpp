/*
	Copyright (C) 2021 TangoCash

	License: GPLv2

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation;

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.

	Included Headers:

	Copyright (c) 2013-14 Mikko Mononen memon@inside.org

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

*/

#include <stdio.h>
#include <string.h>
#include <string>
#include <float.h>
#define NANOSVG_ALL_COLOR_KEYWORDS	// Include full list of color keywords.
#define NANOSVG_IMPLEMENTATION		// Expands implementation
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include <config.h>
#include "pv_config.h"

int fh_svg_id(const char *name)
{
	std::string fn = name;
	if(fn.substr(fn.find_last_of(".") + 1) == "svg")
		return(1);
	else
		return(0);
}

int fh_svg_load(const char *name, unsigned char **buffer, int* xp, int* yp);

int fh_svg_load(const char *name, unsigned char **buffer, int* xp, int* yp)
{
	NSVGimage *image = NULL;
	NSVGrasterizer *rast = NULL;
	int w, h;

	//printf("[SVG] parsing load %s\n", name);
	image = nsvgParseFromFile(name, "px", 96.0f);
	if (image == NULL)
	{
		printf("[SVG] Could not open SVG image.\n");
		goto error;
	}
	w = (int)image->width;
	h = (int)image->height;

	rast = nsvgCreateRasterizer();
	if (rast == NULL)
	{
		printf("[SVG] Could not init rasterizer.\n");
		goto error;
	}

	if(w > *xp || h > *yp)
	{
		free(*buffer);
		*buffer = (unsigned char*) malloc(w*h*4);
	}
	*xp = w;
	*yp = h;

	if (buffer == NULL)
	{
		printf("[SVG] Could not alloc image buffer.\n");
		goto error;
	}

	//printf("[SVG] rasterizing image %d x %d\n", w, h);
	nsvgRasterize(rast, image, 0,0,1, *buffer, w, h, w*4);

error:
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);

	return 0;
}

int svg_load_resize(const char *name, unsigned char **buffer, int* ox, int* oy, int dx, int dy);

int svg_load_resize(const char *name, unsigned char **buffer, int* ox, int* oy, int dx, int dy)
{
	NSVGimage *image = NULL;
	NSVGrasterizer *rast = NULL;
	int w, h;

	//printf("[SVG] parsing load %s\n", name);
	image = nsvgParseFromFile(name, "px", 96.0f);
	if (image == NULL)
	{
		printf("[SVG] Could not open SVG image.\n");
		goto error;
	}
	w = (int)image->width;
	h = (int)image->height;

	rast = nsvgCreateRasterizer();
	if (rast == NULL)
	{
		printf("[SVG] Could not init rasterizer.\n");
		goto error;
	}

	float scale_w,scale_h;
	scale_w = (float)dx/(float)w;
	scale_h = (float)dy/(float)h;

	w = (int)(w*scale_w);
	h = (int)(h*scale_h);

	free(*buffer);
	*buffer = (unsigned char*) malloc(w*h*4);

	*ox = w;
	*oy = h;

	if (buffer == NULL)
	{
		printf("[SVG] Could not alloc image buffer.\n");
		goto error;
	}

	//printf("[SVG] rasterizing image %d x %d\n", w, h);
	nsvgRasterizeFull(rast, image, 0, 0, scale_w, scale_h, *buffer, w, h, w*4);

error:
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);

	return 0;
}

int fh_svg_getsize(const char *name,int *x,int *y, int /*wanted_width*/, int /*wanted_height*/)
{
	NSVGimage *image = NULL;
	int w, h;

	//printf("[SVG] parsing getsize %s\n", name);
	image = nsvgParseFromFile(name, "px", 96.0f);
	if (image == NULL)
	{
		printf("[SVG] Could not open SVG image.\n");
		goto error;
	}
	w = (int)image->width;
	h = (int)image->height;

	*x = w;
	*y = h;

error:
	nsvgDelete(image);
	return 0;
}

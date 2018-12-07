/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2017, Thilo Graf 'dbt'
	Copyright (C) 2012, Michael Liebmann 'micha-bbg'

	License: GPL

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <global.h>
#include <neutrino.h>
#include "cc_draw.h"
#include "cc_timer.h"
#include <cs_api.h>
#include <driver/pictureviewer/pictureviewer.h>
#include <system/debug.h>
extern CPictureViewer * g_PicViewer;

/* export CCDRAW_DEBUG to paint red lines around all elements */
static bool CCDraw_debug = !!(getenv("CCDRAW_DEBUG"));

CCDraw::CCDraw() : COSDFader(g_settings.theme.menu_Content_alpha)
{
	frameBuffer 		= CFrameBuffer::getInstance();

	x = cc_xr = cc_xr_old = x_old	= 0;
	y = cc_yr = cc_yr_old = y_old	= 0;
	height	= height_old	= CC_HEIGHT_MIN;
	width	= width_old	= CC_WIDTH_MIN;

	col_body = col_body_old			= COL_MENUCONTENT_PLUS_0;
	col_shadow = col_shadow_old 		= COL_SHADOW_PLUS_0;
	col_frame = col_frame_old 		= COL_FRAME_PLUS_0;
	col_shadow_clean			= 0;

	cc_body_image = cc_body_image_old 	= std::string();

	fr_thickness = fr_thickness_old		= 0;

	corner_type = corner_type_old		= CORNER_NONE;
	corner_rad = corner_rad_old		= 0;

	shadow			= CC_SHADOW_OFF;
	shadow_w = shadow_w_old	= OFFSET_SHADOW;
	shadow_force		= false;

	cc_paint_cache		= false;
	cc_scrdata.pixbuf	= NULL;
	cc_save_bg		= false;
	firstPaint		= true;
	is_painted		= false;
	force_paint_bg	= false;
	paint_bg 		= true;
	cc_allow_paint		= true;
	cc_enable_frame		= true;

	cc_body_gradient_enable	= cc_body_gradient_enable_old 	= CC_COLGRAD_OFF;
	cc_body_gradient_2nd_col = cc_body_gradient_2nd_col_old	= COL_MENUCONTENT_PLUS_0;

	cc_body_gradient_mode 					= CColorGradient::gradientLight2Dark;
	cc_body_gradient_intensity 				= CColorGradient::light;
	cc_body_gradient_intensity_v_min 			= 0x40;
	cc_body_gradient_intensity_v_max 			= 0xE0;
	cc_body_gradient_saturation 				= 0xC0;
	cc_body_gradient_direction = cc_body_gradient_direction_old	= CFrameBuffer::gradientVertical;

	cc_draw_timer		= NULL;
	cc_draw_trigger_slot 	= sigc::mem_fun0(*this, &CCDraw::paintTrigger);

	cc_gradient_bg_cleanup = true;

	v_fbdata.clear();
}

CCDraw::~CCDraw()
{
	if(cc_draw_timer){
		delete cc_draw_timer; cc_draw_timer = NULL;
	}
	clearFbData();
}

bool CCDraw::applyPosChanges()
{
	bool ret = false;
	if (x != x_old || cc_xr != cc_xr_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], Pos changes x %d != x_old %d... [cc_xr = %d cc_xr_old = %d]\033[0m\n", __func__, __LINE__, x, x_old, cc_xr, cc_xr_old);
		x_old = x;
		cc_xr_old = cc_xr;
		ret = true;
	}
	if (y != y_old || cc_yr != cc_yr_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], Pos changes y %d != y_old %d... [cc_yr = %d cc_yr_old = %d]\033[0m\n", __func__, __LINE__, y, y_old, cc_yr, cc_yr_old);
		y_old = y;
		cc_yr_old = cc_yr;
		ret = true;
	}

	return ret;
}

bool CCDraw::applyDimChanges()
{
	bool ret = false;
	if (height != height_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes height %d != height_old %d...\033[0m\n", __func__, __LINE__, height, height_old);
		height_old = height;
		ret = true;
	}
	if (width != width_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes width %d != width_old %d...\033[0m\n", __func__, __LINE__, width, width_old);
		width_old = width;
		ret = true;
	}
	if (fr_thickness != fr_thickness_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes fr_thickness %d != fr_thickness_old %d...\033[0m\n", __func__, __LINE__, fr_thickness, fr_thickness_old);
		fr_thickness_old = fr_thickness;
		ret = true;
	}
	if (shadow_w != shadow_w_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes shadow_w_sel %d != shadow_w_old %d...\033[0m\n", __func__, __LINE__, shadow_w, shadow_w_old);
		shadow_w_old = shadow_w;
		ret = true;
	}
	if (corner_rad != corner_rad_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes corner_rad %d != corner_rad_old %d...\033[0m\n", __func__, __LINE__, corner_rad, corner_rad_old);
		corner_rad_old = corner_rad;
		ret = true;
	}
	if (corner_type != corner_type_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], dim changes corner_type %d != corner_type_old %d...\033[0m\n", __func__, __LINE__, corner_type, corner_type_old);
		corner_type_old = corner_type;
		ret = true;
	}

	return ret;
}

bool CCDraw::applyColChanges()
{
	bool ret = false;
	if (col_body != col_body_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes col_body %d != col_body_old %d...\033[0m\n", __func__, __LINE__, col_body, col_body_old);
		col_body_old = col_body;
		ret = true;
	}
	if (col_shadow != col_shadow_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes col_shadow %d != col_shadow_old %d...\033[0m\n", __func__, __LINE__, col_shadow, col_shadow_old);
		col_shadow_old = col_shadow;
		ret = true;
	}
	if (col_frame != col_frame_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes col_frame %d != col_frame_old %d...\033[0m\n", __func__, __LINE__, col_frame, col_frame_old);
		col_frame_old = col_frame;
		ret = true;
	}
	if (cc_body_gradient_enable != cc_body_gradient_enable_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes cc_body_gradient_enable %d != cc_body_gradient_enable_old %d...\033[0m\n", __func__, __LINE__, cc_body_gradient_enable, cc_body_gradient_enable_old);
		cc_body_gradient_enable_old = cc_body_gradient_enable;
		ret = true;
	}
	if (cc_body_gradient_2nd_col != cc_body_gradient_2nd_col_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes cc_body_gradient_2nd_col %d != cc_body_gradient_2nd_col_old %d...\033[0m\n", __func__, __LINE__, cc_body_gradient_2nd_col, cc_body_gradient_2nd_col_old);
		cc_body_gradient_2nd_col_old = cc_body_gradient_2nd_col;
		ret = true;
	}
	if (cc_body_gradient_direction != cc_body_gradient_direction_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes cc_body_gradient_direction %d != cc_body_gradient_direction_old %d...\033[0m\n", __func__, __LINE__, cc_body_gradient_direction, cc_body_gradient_direction_old);
		cc_body_gradient_direction_old = cc_body_gradient_direction;
		ret = true;
	}
	if (cc_body_image != cc_body_image_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], col changes cc_body_image %s != cc_body_image_old %s...\033[0m\n", __func__, __LINE__, cc_body_image.c_str(), cc_body_image_old.c_str());
		cc_body_image_old = cc_body_image;
		ret = true;
	}

	return ret;
}

bool CCDraw::hasChanges()
{
	if (applyPosChanges() || applyDimChanges() || applyColChanges())
		return true;

	return false;
}

void CCDraw::setXPos(const int& xpos)
{
	if (x == xpos)
		return;
	x = xpos;
}

void CCDraw::setYPos(const int& ypos)
{
	if (y == ypos)
		return;
	y = ypos;
}

void CCDraw::setHeight(const int& h)
{
	if (height == h)
		return;
	height = h;
}

void CCDraw::setWidth(const int& w)
{
	if (width == w)
		return;
	width = w;
}

void CCDraw::setFrameThickness(const int& thickness)
{
	fr_thickness = thickness;

	//ensure enabled frame if frame width > 0
	cc_enable_frame = false;
	if (fr_thickness)
		cc_enable_frame = true;
}

bool CCDraw::enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color, const int& direction)
{
	if (cc_body_gradient_enable == enable_mode && cc_body_gradient_direction == direction)
		return false;
	dprintf(DEBUG_DEBUG, "\033[33m[CCDraw]\t[%s - %d], change gradient mode: current=[%d] new=[%d] direction=[%d]\033[0m\n", __func__, __LINE__, cc_body_gradient_enable, enable_mode, direction);
	bool ret = false;
	if ((cc_body_gradient_enable != enable_mode) || (cc_body_gradient_enable == CC_COLGRAD_OFF)){
		clearScreenBuffer();
		cc_body_gradient_enable = enable_mode;
		ret = true;
	}
	if (cc_body_gradient_enable == CC_COLGRAD_COL_A_2_COL_B || cc_body_gradient_enable == CC_COLGRAD_COL_B_2_COL_A)
		set2ndColor(sec_color);

	//handle direction
	if (cc_body_gradient_direction != direction){
		cc_body_gradient_direction = direction;
		ret = true;
	}

	return ret;
}

void CCDraw::setCornerType(const int& type)
{
	if (corner_type == type)
		return;
	corner_type = type;
}

void CCDraw::setCorner(const int& radius, const int& type)
{
	setCornerType(type);
	if (corner_rad == radius)
		return;
	corner_rad = radius;
}

gradientData_t* CCDraw::getGradientData()
{
	if (cc_body_gradient_enable == CC_COLGRAD_OFF)
		return NULL;

	gradientData_t* gdata 	= new gradientData_t;
	gdata->gradientBuf 	= NULL;
	gdata->boxBuf 		= NULL;
	gdata->direction 	= cc_body_gradient_direction;
	gdata->mode 		= CFrameBuffer::pbrg_noFree;
	CColorGradient ccGradient;
	int gsize = cc_body_gradient_direction == CFrameBuffer::gradientVertical ? height : width;
	//TODO: add modes for direction and intensity
	switch (cc_body_gradient_enable){
		case  CC_COLGRAD_LIGHT_2_DARK:
			cc_body_gradient_mode = CColorGradient::gradientLight2Dark;
			break;
		case  CC_COLGRAD_DARK_2_LIGHT:
			cc_body_gradient_mode = CColorGradient::gradientDark2Light;
			break;
		case  CC_COLGRAD_COL_A_2_COL_B:
			cc_body_gradient_mode = CColorGradient::gradientLight2Dark;
			break;
		case  CC_COLGRAD_COL_B_2_COL_A:
			cc_body_gradient_mode = CColorGradient::gradientDark2Light;
			break;
		case  CC_COLGRAD_COL_LIGHT_DARK_LIGHT:
			cc_body_gradient_mode = CColorGradient::gradientLight2Dark2Light;
			break;
		case  CC_COLGRAD_COL_DARK_LIGHT_DARK:
			cc_body_gradient_mode = CColorGradient::gradientDark2Light2Dark;
			break;
	}

	if (cc_body_gradient_enable == CC_COLGRAD_COL_A_2_COL_B || cc_body_gradient_enable == CC_COLGRAD_COL_B_2_COL_A){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], init gradient c2c)...\033[0m\n", __func__, __LINE__);
		gdata->gradientBuf = ccGradient.gradientColorToColor(col_body,
								cc_body_gradient_2nd_col,
								NULL,
								gsize,
								cc_body_gradient_mode,
								cc_body_gradient_intensity);
	}else{
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], init gradient single color)...\033[0m\n", __func__, __LINE__);
		gdata->gradientBuf = ccGradient.gradientOneColor(col_body,
							NULL,
							gsize,
							cc_body_gradient_mode,
							cc_body_gradient_intensity,
							cc_body_gradient_intensity_v_min,
							cc_body_gradient_intensity_v_max,
							cc_body_gradient_saturation);
	}

	return gdata;
}

bool CCDraw::clearSavedScreen()
{
	/* Here we clean only screen buffers from background layers.
	 * Paint cache and gradient are not touched.
	*/
	bool ret = false;
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
			if (v_fbdata[i].pixbuf){
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], cleanup bg...\033[0m\n", __func__, __LINE__);
				delete[] v_fbdata[i].pixbuf;
				v_fbdata[i].pixbuf = NULL;
				ret = true;
			}
		}
	}
	return ret;
}

bool CCDraw::clearPaintCache()
{
	/* Here we clean only the paint cache from foreground layers.
	 * BG layer is not touched.
	*/
	bool ret = false;
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].fbdata_type != CC_FBDATA_TYPE_BGSCREEN){
			if (v_fbdata[i].pixbuf){
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], cleanup paint cache layer...\033[0m\n", __func__, __LINE__);
				delete[] v_fbdata[i].pixbuf;
				v_fbdata[i].pixbuf = NULL;
				ret = true;
			}
		}
	}
	return ret;
}

//clean old gradient buffer
bool CCDraw::clearFbGradientData()
{
	bool ret = false;
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].gradient_data){
			if (v_fbdata[i].gradient_data->gradientBuf){
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], clean up gradientBuf   \t %p...\033[0m\n", __func__, __LINE__, v_fbdata[i].gradient_data->gradientBuf);
				free(v_fbdata[i].gradient_data->gradientBuf);
				v_fbdata[i].gradient_data->gradientBuf = NULL;
			}
			if (v_fbdata[i].gradient_data->boxBuf){
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], clean up boxBuf     \t %p...\033[0m\n", __func__, __LINE__, v_fbdata[i].gradient_data->boxBuf);
				cs_free_uncached(v_fbdata[i].gradient_data->boxBuf);
				v_fbdata[i].gradient_data->boxBuf = NULL;
			}
			dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], clean up gradient data \t %p...\033[0m\n", __func__, __LINE__, v_fbdata[i].gradient_data);
			delete v_fbdata[i].gradient_data;
			v_fbdata[i].gradient_data = NULL;
			ret = true;
		}
	}
	return ret;
}

bool CCDraw::clearScreenBuffer()
{
	bool ret = false;

	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].pixbuf){
			dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], cleanup pixbuf...\033[0m\n", __func__, __LINE__);
			delete[] v_fbdata[i].pixbuf;
			v_fbdata[i].pixbuf = NULL;
			ret = true;
		}
	}
	if (clearFbGradientData())
		ret = true;

	firstPaint = true;
	return ret;
}

void CCDraw::clearFbData()
{
	clearScreenBuffer();
	v_fbdata.clear();
}

bool CCDraw::CheckFbData(const cc_fbdata_t& fbdata, const char* func, const int line)
{
	int32_t rows = fbdata.dx / (int32_t)frameBuffer->getScreenWidth(true) - 1 + fbdata.y;
	int32_t rest = fbdata.dx % (int32_t)frameBuffer->getScreenWidth(true);
        int32_t end  = rows * (int32_t)frameBuffer->getScreenWidth(true) + rest;
	if (	(fbdata.x < 0 || fbdata.y < 0) ||
		(end >= (int32_t)frameBuffer->getScreenWidth(true)*(int32_t)frameBuffer->getScreenHeight(true)) 
	   ) {
		dprintf(DEBUG_NORMAL, "[CCDraw] ERROR! Position < 0 or > FB end [%s - %d]\n\tx = %d  y = %d\n\tdx = %d  dy = %d\n item: %s [type: %d]\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy,
				cc_item_type.name.c_str(),
				cc_item_type.id
		);
			return false;
		}
		if (fbdata.dx == 0 || fbdata.dy == 0) {
			dprintf(DEBUG_DEBUG,"[CCDraw]\t[%s - %d], INFO! dx and/or dy = 0, tx = %d,  y = %d, dx = %d,  dy = %d\n item: %s [type: %d]\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy,
				cc_item_type.name.c_str(),
				cc_item_type.id
			);
			return false;
		}
	return true;
}



//screen area save
fb_pixel_t* CCDraw::getScreen(int ax, int ay, int dx, int dy)
{
	fb_pixel_t* pixbuf = NULL;

	if (dx < 1 ||  dy < 1 || dx * dy == 0)
		return NULL;
	else
		pixbuf = new fb_pixel_t[dx * dy];

	frameBuffer->waitForIdle("CCDraw::getScreen()");
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

cc_screen_data_t CCDraw::getScreenData(const int& ax, const int& ay, const int& dx, const int& dy)
{
	cc_screen_data_t res;
	res.x = res.y = res.dx = res.dy = 0;
	res.pixbuf = getScreen(ax, ay, dx, dy);

	if (res.pixbuf){
		res.x = ax; res.y = ay; res.dx = dx; res.dy = dy;
	}
	else
		dprintf(DEBUG_NORMAL, "\033[33m[CCDraw]\[%s - %d], Warning: initialize of screen buffer failed!\033[0m\n", __func__, __LINE__);

	return res;
}

void CCDraw::enableSaveBg(bool save_bg)
{
	if (!cc_save_bg || (cc_save_bg != save_bg))
		clearSavedScreen();
	cc_save_bg = save_bg;
}

void CCDraw::enablePaintCache(bool enable)
{
	if (!cc_paint_cache || (cc_paint_cache != enable))
		clearPaintCache();
	cc_paint_cache = enable;
}

//paint framebuffer layers
void CCDraw::paintFbItems(bool do_save_bg)
{
	//Pick up signal if filled and execute slots.
	OnBeforePaintLayers();

	//First we modify background handling.
	enableSaveBg(do_save_bg);

	//Save background before first paint, cc_save_bg must be true.
	if (firstPaint && cc_save_bg){
		/* On first we must ensure that screen buffer is empty.
		 * Here we clean possible screen buffers in bg layers,
		 * without paint cache and gradient buffer.
		*/
		clearSavedScreen();

		/* On second step we check for
		 * usable item dimensions and exit here if found any problem.
		*/
		for(size_t i=0; i<v_fbdata.size(); i++){
			if (!CheckFbData(v_fbdata[i], __func__, __LINE__)){
				break;
			}

			/* Here we save the background of current box before paint.
			* Only the reserved fbdata type CC_FBDATA_TYPE_BGSCREEN is here required and is used for this.
			* This pixel buffer is required for the hide() method that will
			* call the restore method from framebuffer class to restore
			* background.
			*/
			if (v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
				v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
				break;
			}
		}
		firstPaint = false;
	}
	
	for(size_t i=0; i< v_fbdata.size(); i++){
		int fbtype = v_fbdata[i].fbdata_type;

		//ignore bg screen layer
		if (fbtype == CC_FBDATA_TYPE_BGSCREEN)
			continue;

		// Don't paint on dimension or position error dx or dy are 0.
		if (!CheckFbData(v_fbdata[i], __func__, __LINE__))
			continue;

		/* Paint all fb relevant basic parts (shadow, frame and body)
		 * with all specified properties, paint_bg must be enabled.
		*/
		if (cc_enable_frame && cc_body_image.empty()){
			if (fbtype == CC_FBDATA_TYPE_FRAME) {
				if (v_fbdata[i].frame_thickness > 0 && cc_allow_paint){
					frameBuffer->paintBoxFrame(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].frame_thickness, v_fbdata[i].color, v_fbdata[i].r, v_fbdata[i].rtype);
					v_fbdata[i].is_painted = true;
				}
				continue;
			}
		}
		if (paint_bg){
			if (fbtype == CC_FBDATA_TYPE_BACKGROUND){
				frameBuffer->paintBackgroundBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
				v_fbdata[i].is_painted = true;
				if (CCDraw_debug)
					frameBuffer->paintBoxFrame(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, 1, COL_RED);
				continue;
			}
		}
		if (fbtype == CC_FBDATA_TYPE_SHADOW_BOX && ((!is_painted || !v_fbdata[i].is_painted)|| shadow_force || force_paint_bg)) {
			if (v_fbdata[i].enabled) {
				/* Here we paint the shadow around the body.
				* On 1st step we check for already cached screen buffer, if true
				* then restore this instead to call the paint methode.
				* This could be usally, if we use an existant instances of "this" object
				*/
				if (cc_allow_paint){
					if (v_fbdata[i].pixbuf){
						dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint shadow from cache...\033[0m\n", __func__, __LINE__);
						frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
					}else{
						frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].color, v_fbdata[i].r, v_fbdata[i].rtype);
					}
					//If is paint cache enabled, catch screen into cache
					if (cc_paint_cache && v_fbdata[i].pixbuf == NULL)
						v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
					v_fbdata[i].is_painted = true;
				}
				continue;
			}
		}
		if (paint_bg){
			if (fbtype == CC_FBDATA_TYPE_BOX){
				if(cc_allow_paint) {
					/* Here we paint the main body of box.
					* On 1st step we check for already cached background buffer, if true
					* then restore this instead to call the paint methodes and gradient creation.
					* Paint cache can be enable/disable with enablePaintCache()
					*/
					if (v_fbdata[i].pixbuf){
						 /* If is paint cache enabled and cache is filled, it's prefered to paint
						  * from cache. Cache is also filled if body background images are used
						 */
						dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint body from cache...\033[0m\n", __func__, __LINE__);
						frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
					}else{
						//Ensure clean gradient data on disabled gradient.
						if(cc_body_gradient_enable == CC_COLGRAD_OFF && v_fbdata[i].gradient_data){
							dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], gradient mode is disabled but filled\033[0m\n", __func__, __LINE__);
							clearFbGradientData();
						}

						/* If background image is defined,
						*  we try to render an image instead to render default box.
						*  Paint of background image is prefered, next steps will be ignored!
						*/
						if (!cc_body_image.empty()){
							if (g_PicViewer->DisplayImage(cc_body_image, v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy)){
								// catch screen and store into paint cache
								v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
								v_fbdata[i].is_painted = true;
							}else{
								if (v_fbdata[i].pixbuf){
									delete[] v_fbdata[i].pixbuf;
									v_fbdata[i].pixbuf = NULL;
								}
								v_fbdata[i].is_painted = false;
							}

							// On failed image paint, write this into log and reset image name.
							if (!v_fbdata[i].is_painted){
								dprintf(DEBUG_NORMAL, "\033[33m\[CCDraw]\t[%s - %d], WARNING: bg image %s defined, but paint failed,\nfallback to default rendering...\033[0m\n", __func__, __LINE__, cc_body_image.c_str());
								cc_body_image = "";
							}
						}

						/* If no background image is defined, we paint default box or box with gradient
						 * This is also possible if any background image is defined but image paint ist failed
						 */
						if (cc_body_image.empty()){
							if (cc_body_gradient_enable != CC_COLGRAD_OFF ){

								/* If color gradient enabled we create a gradient_data
								* instance and add it to the fbdata object
								* On disabled color gradient or image paint was failed, we do paint only a default box
								*/
								if (v_fbdata[i].gradient_data == NULL){
									dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], create new gradient data)...\033[0m\n", __func__, __LINE__);
									v_fbdata[i].gradient_data = getGradientData();
								}

								if (v_fbdata[i].gradient_data->boxBuf == NULL){
									if (v_fbdata[i].pixbuf == NULL){
										/* Before we paint any gradient box with hw acceleration, we must cleanup first.
										* FIXME: This is only a workaround for this framebuffer behavior on enabled hw acceleration.
										* Without this, ugly ghost letters or ghost images inside gradient boxes are possible.
										*/
										if (cc_gradient_bg_cleanup)
											frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, 0, v_fbdata[i].r, v_fbdata[i].rtype);

										// create gradient buffer and paint gradient box
										v_fbdata[i].gradient_data->boxBuf = frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, 0, v_fbdata[i].gradient_data, v_fbdata[i].r, v_fbdata[i].rtype);
										dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint and cache new gradient into gradient cache...\033[0m\n", __func__, __LINE__);
									}

									/* On enabled paint cache or clean up, catch the screen into paint cache and clean up unused gradient buffer.
									* If we don't do this explicit, gradient cache is used.
									*/
									if (cc_paint_cache || cc_gradient_bg_cleanup){
										dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], cache new created gradient into external cache...\033[0m\n", __func__, __LINE__);
										v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
										if (clearFbGradientData())
											dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], remove unused gradient data...\033[0m\n", __func__, __LINE__);
									}
								}else{
									// If found gradient buffer, paint box from gradient cache.
									if (frameBuffer->checkFbArea(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, true)){
										dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint cached gradient)...\033[0m\n", __func__, __LINE__);
										frameBuffer->blitBox2FB(v_fbdata[i].gradient_data->boxBuf, v_fbdata[i].gradient_data->dx, v_fbdata[i].dy, v_fbdata[i].gradient_data->x, v_fbdata[i].y);
										frameBuffer->checkFbArea(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, false);
									}
								}
							}else{
								/* If is nothihng cached or no background image was defined or image paint was failed,
								*  render a default box.
								*/
								dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint default box)...\033[0m\n", __func__, __LINE__);
								frameBuffer->paintBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].color, v_fbdata[i].r, v_fbdata[i].rtype);

								//If is paint cache enabled, catch screen into cache.
								if (cc_paint_cache)
									v_fbdata[i].pixbuf = getScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
							}
						}
					}
					v_fbdata[i].is_painted = true;
					OnAfterPaintBg();
				}
			}
		}
		if (CCDraw_debug)
			frameBuffer->paintBoxFrame(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, 1, COL_RED);
	}

	//set is_painted attribut. if any layer was painted set it to true;
	if (force_paint_bg){
		is_painted = false;
	}else{
		for(size_t i=0; i< v_fbdata.size(); i++){
			if (v_fbdata[i].is_painted){
				is_painted = true;
				break;
			}
		}
	}

	//reset is painted ignore flag to default value
	force_paint_bg = false;

	OnAfterPaintLayers();
}

bool CCDraw::isPainted()
{
	return is_painted;
}

void CCDraw::hide()
{
	OnBeforeHide();
	//restore saved screen background of item if available
	for(size_t i =0; i< v_fbdata.size() ;i++) {
		if (v_fbdata[i].fbdata_type == CC_FBDATA_TYPE_BGSCREEN){
			if (v_fbdata[i].pixbuf) {
				//restore screen from backround layer
				frameBuffer->waitForIdle("CCDraw::hide()");
				frameBuffer->RestoreScreen(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy, v_fbdata[i].pixbuf);
				v_fbdata[i].is_painted = false;
			}
		}
	}
	firstPaint = true;
	is_painted = false;
	OnAfterHide();
}

//erase or paint over rendered objects
void CCDraw::kill(const fb_pixel_t& bg_color, const int& corner_radius, const int& fblayer_type /*fbdata_type*/)
{
	int layers = fblayer_type;

	if (fblayer_type & ~CC_FBDATA_TYPES)
		layers = CC_FBDATA_TYPES;

	for(size_t i =0; i< v_fbdata.size() ;i++){
		if (v_fbdata[i].fbdata_type & layers){

			int r = 0;

			if (corner_radius > -1){
				r = v_fbdata[i].r;
				if (corner_radius != v_fbdata[i].r)
					r = corner_radius;
			}

			if (v_fbdata[i].dx > 0 && v_fbdata[i].dy > 0){
				frameBuffer->paintBoxRel(v_fbdata[i].x,
							v_fbdata[i].y,
							v_fbdata[i].dx,
							v_fbdata[i].dy,
							bg_color,
							r,
							v_fbdata[i].rtype);

				if (v_fbdata[i].fbdata_type & CC_FBDATA_TYPE_FRAME){
					if (v_fbdata[i].frame_thickness)
							frameBuffer->paintBoxFrame(v_fbdata[i].x,
										v_fbdata[i].y,
										v_fbdata[i].dx,
										v_fbdata[i].dy,
										v_fbdata[i].frame_thickness,
										bg_color,
										v_fbdata[i].r,
										v_fbdata[i].rtype);
					}
			}else
				dprintf(DEBUG_DEBUG, "\033[33m[CCDraw]\t[%s - %d] WARNING! render with bad dimensions [dx = %d dy = %d]\033[0m\n", __func__, __LINE__, v_fbdata[i].dx, v_fbdata[i].dy );

			v_fbdata[i].is_painted = false;
		}
	}

	firstPaint = true;
	is_painted = false;
}

void CCDraw::killShadow(const fb_pixel_t& bg_color, const int& corner_radius)
{
	kill(bg_color, corner_radius, CC_FBDATA_TYPE_SHADOW_BOX);
}

bool CCDraw::doPaintBg(bool do_paint)
{
	if (paint_bg == do_paint)
		return false;
	
	paint_bg = do_paint;
	//clearSavedScreen();
	return true;
}

void CCDraw::enableShadow(int mode, const int& shadow_width, bool force_paint)
{
	if (shadow != mode){
		killShadow();
		shadow = mode;
	}
	if (shadow != CC_SHADOW_OFF)
		if (shadow_width != -1)
			setShadowWidth(shadow_width);
	shadow_force = force_paint;
}

void CCDraw::paintTrigger()
{
	if (is_painted)
		hide();
	else
		paint();
}

bool CCDraw::paintBlink(CComponentsTimer* Timer)
{
	if (Timer){
		Timer->OnTimer.connect(cc_draw_trigger_slot);
		return Timer->isRun();
	}
	return false;
}

bool CCDraw::paintBlink(const int& interval, bool is_nano)
{
	if (cc_draw_timer == NULL)
		cc_draw_timer = new CComponentsTimer(interval, is_nano);
	cc_draw_timer->setThreadName(__func__);

	return paintBlink(cc_draw_timer);
}

bool CCDraw::cancelBlink(bool keep_on_screen)
{
	bool res = false;

	if (cc_draw_timer){
		res = cc_draw_timer->stopTimer();
		delete cc_draw_timer; cc_draw_timer = NULL;
	}

	if(keep_on_screen)
		paint1();
	else
		hide();
		

	return res;
}

bool CCDraw::setBodyBGImage(const std::string& image_path)
{
	if (cc_body_image == image_path)
		return false;

	cc_body_image = image_path;

	if (clearPaintCache())
		dprintf(DEBUG_NORMAL, "\033[33m\[CCDraw]\t[%s - %d],  new body background image defined: %s , \033[0m\n", __func__, __LINE__, cc_body_image.c_str());

	return true;
}

bool CCDraw::setBodyBGImageName(const std::string& image_name)
{
	return  setBodyBGImage(frameBuffer->getIconPath(image_name));
}

int  CCDraw::getXPos()
{
	return x;
}

int  CCDraw::getYPos()
{
	return y;
}

int  CCDraw::getHeight()
{
	return height;
}

int  CCDraw::getWidth()
{
	return width;
}

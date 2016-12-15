/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2015, Thilo Graf 'dbt'
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
#include <cs_api.h>

#include <system/debug.h>

CCDraw::CCDraw() : COSDFader(g_settings.theme.menu_Content_alpha)
{
	frameBuffer 		= CFrameBuffer::getInstance();

	x = cc_xr = x_old	= 0;
	y = cc_yr = y_old	= 0;
	height	= height_old	= CC_HEIGHT_MIN;
	width	= width_old	= CC_WIDTH_MIN;

	col_body = col_body_old			= COL_MENUCONTENT_PLUS_0;
	col_shadow = col_shadow_old 		= COL_SHADOW_PLUS_0;
	col_frame = col_frame_old 		= COL_FRAME_PLUS_0;
	col_shadow_clean			= 0;

	fr_thickness = fr_thickness_old		= 0;

	corner_type = corner_type_old		= CORNER_ALL;
	corner_rad = corner_rad_old		= 0;

	shadow			= CC_SHADOW_OFF;
	shadow_w = shadow_w_old	= OFFSET_SHADOW;
	shadow_force		= false;

	cc_paint_cache		= false;
	cc_scrdata.pixbuf	= NULL;
	cc_save_bg		= false;
	firstPaint		= true;
	is_painted		= false;
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

inline bool CCDraw::applyPosChanges()
{
	bool ret = false;
	if (x != x_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], Pos changes x %d != x_old %d...\033[0m\n", __func__, __LINE__, x, x_old);
		x_old = x;
		ret = true;
	}
	if (y != y_old){
		dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], Pos changes y %d != y_old %d...\033[0m\n", __func__, __LINE__, y, y_old);
		y_old = y;
		ret = true;
	}

	return ret;
}

inline bool CCDraw::applyDimChanges()
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

inline bool CCDraw::applyColChanges()
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

	return ret;
}

inline bool CCDraw::hasChanges()
{
	if (applyPosChanges() || applyDimChanges() || applyColChanges())
		return true;

	return false;
}

inline void CCDraw::setXPos(const int& xpos)
{
	if (x == xpos)
		return;
	x = xpos;
}

inline void CCDraw::setYPos(const int& ypos)
{
	if (y == ypos)
		return;
	y = ypos;
}

inline void CCDraw::setHeight(const int& h)
{
	if (height == h)
		return;
	height = h;
}

inline void CCDraw::setWidth(const int& w)
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

inline void CCDraw::setCornerType(const int& type)
{
	if (corner_type == type)
		return;
	corner_type = type;
}

inline void CCDraw::setCorner(const int& radius, const int& type)
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
				free(v_fbdata[i].gradient_data->gradientBuf);
				v_fbdata[i].gradient_data->gradientBuf = NULL;
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], clean up gradientBuf...\033[0m\n", __func__, __LINE__);
			}
			if (v_fbdata[i].gradient_data->boxBuf){
				cs_free_uncached(v_fbdata[i].gradient_data->boxBuf);
				v_fbdata[i].gradient_data->boxBuf = NULL;
				dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], clean up boxBuf...\033[0m\n", __func__, __LINE__);
			}
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
	if (clearSavedScreen())
		ret = true;
	if (clearPaintCache())
		ret = true;
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
			dprintf(DEBUG_NORMAL, "[CCDraw] ERROR! Position < 0 or > FB end [%s - %d]\n\tx = %d  y = %d\n\tdx = %d  dy = %d\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy);
			return false;
		}
		if (fbdata.dx == 0 || fbdata.dy == 0) {
			dprintf(DEBUG_DEBUG,"[CCDraw]\t[%s - %d], INFO! dx and/or dy = 0, tx = %d,  y = %d, dx = %d,  dy = %d\n",
				func, line,
				fbdata.x, fbdata.y,
				fbdata.dx, fbdata.dy);
			return false;
		}
	return true;
}



//screen area save
fb_pixel_t* CCDraw::getScreen(int ax, int ay, int dx, int dy)
{
	if (dx < 1 ||  dy < 1 || dx * dy == 0)
		return NULL;

	dprintf(DEBUG_INFO, "[CCDraw] INFO! [%s - %d], ax = %d, ay = %d, dx = %d, dy = %d\n", __func__, __LINE__, ax, ay, dx, dy);
	fb_pixel_t* pixbuf = new fb_pixel_t[dx * dy];
	frameBuffer->waitForIdle("CCDraw::getScreen()");
	frameBuffer->SaveScreen(ax, ay, dx, dy, pixbuf);
	return pixbuf;
}

cc_screen_data_t CCDraw::getScreenData(const int& ax, const int& ay, const int& dx, const int& dy)
{
	cc_screen_data_t res;
	res.pixbuf = getScreen(ax, ay, dx, dy);
	res.x = ax; res.y = ay; res.dx = dx; res.dy = dy;

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
	//pick up signal if filled
	OnBeforePaintLayers();

	//first modify background handling
	enableSaveBg(do_save_bg);

	//save background before first paint, cc_save_bg must be true
	if (firstPaint && cc_save_bg){
		/* On first we must ensure that screen buffer is empty.
		 * Here we clean possible screen buffers in bg layers,
		 * without paint cache and gradient buffer.
		*/
		clearSavedScreen();

		/* On second step we check for
		 * usable item dimensions and exit here if found any problem
		*/
		for(size_t i=0; i<v_fbdata.size(); i++){
			if (!CheckFbData(v_fbdata[i], __func__, __LINE__)){
				break;
			}

			dprintf(DEBUG_DEBUG, "[CCDraw]\n\t[%s - %d] firstPaint->save screen: %d, fbdata_type: %d\n\tx = %d\n\ty = %d\n\tdx = %d\n\tdy = %d\n",
			__func__,
			__LINE__,
			firstPaint,
			v_fbdata[i].fbdata_type,
			v_fbdata[i].x,
			v_fbdata[i].y,
			v_fbdata[i].dx,
			v_fbdata[i].dy);

			/* here we save the background of current box before paint.
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
		cc_fbdata_t& fbdata = v_fbdata[i];

		// Don't paint on dimension or position error dx or dy are 0
		if (!CheckFbData(fbdata, __func__, __LINE__)){
			continue;
		}
		int fbtype = fbdata.fbdata_type;

			dprintf(DEBUG_DEBUG, "[CCDraw]\n\t[%s - %d], fbdata_[%d]\n\tx = %d\n\ty = %d\n\tdx = %d\n\tdy = %d\n",
			__func__,
			__LINE__,
			(int)i,
			fbdata.x,
			fbdata.y,
			fbdata.dx,
			fbdata.dy);

		/*paint all fb relevant basic parts (shadow, frame and body)
		 * with all specified properties, paint_bg must be enabled
		*/
		if (cc_enable_frame){
			if (fbtype == CC_FBDATA_TYPE_FRAME) {
				if (fbdata.frame_thickness > 0 && cc_allow_paint){
					frameBuffer->paintBoxFrame(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, fbdata.frame_thickness, fbdata.color, fbdata.r, fbdata.rtype);
					v_fbdata[i].is_painted = true;
				}
			}
		}
		if (paint_bg){
			if (fbtype == CC_FBDATA_TYPE_BACKGROUND){
				frameBuffer->paintBackgroundBoxRel(x, y, fbdata.dx, fbdata.dy);
				v_fbdata[i].is_painted = true;
			}
		}
		if (fbtype == CC_FBDATA_TYPE_SHADOW_BOX && ((!is_painted || !fbdata.is_painted)|| shadow_force)) {
			if (fbdata.enabled) {
				/* here we paint the shadow around the body
					* on 1st step we check for already cached screen buffer, if true
					* then restore this instead to call the paint methode.
					* This could be usally, if we use existant instances of "this" object
				*/
				if (cc_allow_paint){
					if (fbdata.pixbuf){
						dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint shadow from cache...\033[0m\n", __func__, __LINE__);
						frameBuffer->RestoreScreen(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, fbdata.pixbuf);
					}else{
						frameBuffer->paintBoxRel(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, fbdata.color, fbdata.r, fbdata.rtype);
					}
					//if is paint cache enabled
					if (cc_paint_cache && fbdata.pixbuf == NULL)
						fbdata.pixbuf = getScreen(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy);
					fbdata.is_painted = true;
				}
			}
		}
		if (paint_bg){
			if (fbtype == CC_FBDATA_TYPE_BOX){
				if(cc_allow_paint) {
					/* here we paint the main body of box
					* on 1st step we check for already cached background buffer, if true
					* then restore this instead to call the paint methodes and gradient creation
					* paint cache can be enable/disable with enablePaintCache()
					*/
					if (fbdata.pixbuf){
						dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint body from cache...\033[0m\n", __func__, __LINE__);
						frameBuffer->RestoreScreen(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, fbdata.pixbuf);
					}else{
						//ensure clean gradient data on disabled gradient
						if(cc_body_gradient_enable == CC_COLGRAD_OFF && fbdata.gradient_data){
							dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], gradient mode is disabled but filled\033[0m\n", __func__, __LINE__);
							clearFbGradientData();
						}
						if (cc_body_gradient_enable != CC_COLGRAD_OFF){
							/* if color gradient enabled we create a gradient_data
							* instance and add it to the fbdata object
							* On disabled coloor gradient we do paint only a default box
							*/
							if (fbdata.gradient_data == NULL){
								dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], crate new gradient data)...\033[0m\n", __func__, __LINE__);
								fbdata.gradient_data = getGradientData();
							}

							// if found empty gradient buffer, create it, otherwise paint from gradient cache
							if (fbdata.gradient_data->boxBuf == NULL){
								if (!fbdata.pixbuf){
									// on enabled clean up, paint blank screen before create gradient box, this prevents possible ghost text with hw acceleration
									if (cc_gradient_bg_cleanup)
										frameBuffer->paintBoxRel(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, 0, fbdata.r, fbdata.rtype);

									// create gradient buffer and paint gradient box
									fbdata.gradient_data->boxBuf = frameBuffer->paintBoxRel(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, 0, fbdata.gradient_data, fbdata.r, fbdata.rtype);
									dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint and cache new gradient into gradient cache...\033[0m\n", __func__, __LINE__);
								}

								/* On enabled paint cache or clean up, catch the screen into paint cache and clean up unused gradient buffer.
								 * If we don't do this, gradient cache is used.
								*/
								if (cc_paint_cache || cc_gradient_bg_cleanup){
									dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], cache new created gradient into external cache...\033[0m\n", __func__, __LINE__);
									fbdata.pixbuf = getScreen(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy);
									if (clearFbGradientData())
										dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], remove unused gradient data...\033[0m\n", __func__, __LINE__);
								}
							}else{
								//use gradient cache to repaint gradient box
								dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint cached gradient)...\033[0m\n", __func__, __LINE__);
								frameBuffer->checkFbArea(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, true);
								frameBuffer->blitBox2FB(fbdata.gradient_data->boxBuf, fbdata.gradient_data->dx, fbdata.dy, fbdata.gradient_data->x, fbdata.y);
								frameBuffer->checkFbArea(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, false);
							}
						}else{
							dprintf(DEBUG_INFO, "\033[33m[CCDraw]\t[%s - %d], paint default box)...\033[0m\n", __func__, __LINE__);
							frameBuffer->paintBoxRel(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy, fbdata.color, fbdata.r, fbdata.rtype);
							if (cc_paint_cache)
								fbdata.pixbuf = getScreen(fbdata.x, fbdata.y, fbdata.dx, fbdata.dy);
						}
					}
					is_painted = v_fbdata[i].is_painted = true;
				}
			}
		}
	}
	//pick up signal if filled
	OnAfterPaintLayers();
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
	is_painted = false;
	firstPaint = true;
	OnAfterHide();
}

//erase or paint over rendered objects
void CCDraw::kill(const fb_pixel_t& bg_color, const int& corner_radius, const int& fblayer_type /*fbdata_type*/)
{
	for(size_t i =0; i< v_fbdata.size() ;i++){
		if (fblayer_type == CC_FBDATA_TYPES || v_fbdata[i].fbdata_type & fblayer_type){
#if 0
		if (bg_color != COL_BACKGROUND_PLUS_0)
#endif
			int r =  v_fbdata[i].r;
			if (corner_radius > -1)
				r = corner_radius;

			if (v_fbdata[i].dx > 0 && v_fbdata[i].dy > 0){
				frameBuffer->paintBoxRel(v_fbdata[i].x,
							v_fbdata[i].y,
							v_fbdata[i].dx,
							v_fbdata[i].dy,
							bg_color,
							r,
							corner_type);
			}else
				dprintf(DEBUG_DEBUG, "\033[33m[CCDraw][%s - %d], WARNING! render with bad dimensions [dx = %d dy = %d]\033[0m\n", __func__, __LINE__, v_fbdata[i].dx, v_fbdata[i].dy );

			if (v_fbdata[i].frame_thickness)
					frameBuffer->paintBoxFrame(v_fbdata[i].x,
								   v_fbdata[i].y,
								   v_fbdata[i].dx,
								   v_fbdata[i].dy,
								   v_fbdata[i].frame_thickness,
								   bg_color,
								   r,
								   corner_type);
			v_fbdata[i].is_painted = false;
#if 0
		else
			frameBuffer->paintBackgroundBoxRel(v_fbdata[i].x, v_fbdata[i].y, v_fbdata[i].dx, v_fbdata[i].dy);
#endif
		}
	}

	if (fblayer_type == CC_FBDATA_TYPES){
		firstPaint = true;
		is_painted = false;
	}
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
	if (!is_painted)
		paint1();
	else
		hide();
}

bool CCDraw::paintBlink(const int& interval, bool is_nano)
{
	if (cc_draw_timer == NULL){
		cc_draw_timer = new CComponentsTimer(interval, is_nano);
		if (cc_draw_timer->OnTimer.empty()){
			cc_draw_timer->OnTimer.connect(cc_draw_trigger_slot);
		}
	}
	if (cc_draw_timer)
		return cc_draw_timer->isRun();

	return false;
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
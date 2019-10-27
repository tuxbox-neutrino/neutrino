/*
	Based up Neutrino-GUI - Tuxbox-Project
	Copyright (C) 2001 by Steffen Hehn 'McClean'

	Classes for generic GUI-related components.
	Copyright (C) 2012-2018, Thilo Graf 'dbt'
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

#ifndef __CC_DRAW__
#define __CC_DRAW__

#include "cc_types.h"
#include "cc_signals.h"
#include <driver/colorgradient.h>
#include <driver/framebuffer.h>
#include <driver/fade.h>
#include <gui/color.h>

/// Basic component class.
/*!
Basic paint attributes and member functions for component classes
*/

class CComponentsTimer;
class CCDraw : public COSDFader, public CComponentsSignals, public CCTypes
{
	protected:
		///pixel buffer handling, returns pixel buffer depends of given parameters
		fb_pixel_t* getScreen(int ax, int ay, int dx, int dy);
		///returns screen data as screen_data_t
		cc_screen_data_t getScreenData(const int& ax, const int& ay, const int& dx, const int& dy);
		cc_screen_data_t cc_scrdata;

		///object: framebuffer object, usable in all sub classes
		CFrameBuffer * frameBuffer;

		///internal draw timer, used for effects
		CComponentsTimer *cc_draw_timer;
		///slot for timer event, reserved for cc_draw_timer
		sigc::slot0<void> cc_draw_trigger_slot;
		///paint item with trigger effect
		virtual void paintTrigger();

		///property: x-position on screen, to alter with setPos() or setDimensionsAll(), see also defines CC_APPEND, CC_CENTERED
		int x, x_old;
		///property: y-position on screen, to alter setPos() or setDimensionsAll(), see also defines CC_APPEND, CC_CENTERED
		int y, y_old;
		///property: contains real x-position on screen
		int cc_xr, cc_xr_old;
		///property: contains real y-position on screen
		int cc_yr, cc_yr_old;
		///property: height-dimension on screen, to alter with setHeight() or setDimensionsAll()
		int height, height_old;
		///property: width-dimension on screen, to alter with setWidth() or setDimensionsAll()
		int width, width_old;

		///property: color of body
		fb_pixel_t col_body, col_body_old;
		///property: color of shadow
		fb_pixel_t col_shadow, col_shadow_old;
		///property: color of frame
		fb_pixel_t col_frame, col_frame_old;
		///internal property: color for shadow clean up
		fb_pixel_t col_shadow_clean;
		///property: background image, see also setBodyBGImage()
		std::string cc_body_image, cc_body_image_old;

		 ///property: frame thickness, see also setFrameThickness()
		int fr_thickness, fr_thickness_old;

		///property: has corners with definied type, types are defined in /driver/frambuffer.h, without effect, if corner_radius=0
		int corner_type, corner_type_old;
		///property: defined radius of corner, without effect, if corner_type=0
		int corner_rad, corner_rad_old;

		///property: shadow mode 0 = CC_SHADOW_OFF
		int shadow;
		///property: width of shadow
		int shadow_w, shadow_w_old;
		///property: force shadow paint, see enableShadow()
		bool shadow_force;

		///returns true if internal property was changed
		virtual bool hasChanges();
		///apply current position changes and returns true if internal values were changed
		bool applyPosChanges();
		///apply current dimension changes and returns true if internal values were changed
		bool applyDimChanges();
		///apply current color changes and returns true if internal values were changed
		bool applyColChanges();

		///paint caching for body and shadow, default init value = true, see also enablePaintCache() NOTE: has no effect if paint_bg = false
		bool cc_paint_cache;
		
		///enable/disable background buffer, default init value = false, see also enableSaveBg()
		bool cc_save_bg;

		///container: for frambuffer properties and pixel buffer
		std::vector<cc_fbdata_t> v_fbdata;

		///status: true=component was painted for 1st time
		bool firstPaint;
		///status: true=component was rendered
		bool is_painted;
		///status: true= value is_painted would be ignored
		bool force_paint_bg;
		///mode: true=activate rendering of basic elements (frame, shadow and body)
		bool paint_bg;
		///mode: true=activate rendering of frame
		bool cc_enable_frame;
		///mode:  true=allows painting of item, see also allowPaint()
		bool cc_allow_paint;

		///property: true component can paint gradient, see also enableColBodyGradient() TODO: if possible, merge all gradient properties into only one variable
		int cc_body_gradient_enable, cc_body_gradient_enable_old;
		///property: background gradient mode
		int cc_body_gradient_mode;
		///property: background gradient intensity
		int cc_body_gradient_intensity;
		///property: background gradient intensity value min
		uint8_t cc_body_gradient_intensity_v_min;
		///property: background gradient intensity value max
		uint8_t cc_body_gradient_intensity_v_max;
		///property: background gradient saturation
		uint8_t cc_body_gradient_saturation;
		///property: background gradient direction
		int cc_body_gradient_direction, cc_body_gradient_direction_old;

		///property: background gradient 2nd color
		fb_pixel_t cc_body_gradient_2nd_col, cc_body_gradient_2nd_col_old;

		///check current fbdtata position and dimensions, parameter fbdata is an element of v_fbdata, returns false on error
		bool CheckFbData(const cc_fbdata_t& fbdata, const char* func, const int line);

		///sub: get gradient data evaluted with current parameters
		gradientData_t* getGradientData();

		bool cc_gradient_bg_cleanup;

		///rendering of framebuffer elements at once,
		///elements are contained in v_fbdata, presumes added frambuffer elements with paintInit(),
		///parameter do_save_bg=true, saves background of element to pixel buffer, this can be restore with hide()
		void paintFbItems(const bool &do_save_bg = true);

	public:
		///basic component class constructor.
		CCDraw();
		virtual~CCDraw();

		///cleans saved screen buffer, required by hide(), returns true if any buffer was deleted
		bool clearSavedScreen();
		///cleanup paint cache, removes saved buffer contents from cached foreground layers, returns true if any buffer was removed
		bool clearPaintCache();
		///cleanup old gradient buffers, returns true if any gradient buffer data was removed
		bool clearFbGradientData();

		///cleans all possible screen buffers, it calls clearSavedScreen(), clearPaintCache() and  clearFbGradientData() at once
		bool clearScreenBuffer();
		///does the same like clearScreenBuffer(), additional cleans v_fbdata layers and reset layer properties
		void clearFbData();

		///set screen x-position, parameter as int
		virtual void setXPos(const int& xpos);
		///set screen y-position, parameter as int
		virtual void setYPos(const int& ypos);
		///set x and y position at once
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to set the real screen position, look at setRealPos()
		virtual void setPos(const int& xpos, const int& ypos);

		///set height of component on screen
		virtual void setHeight(const int& h);
		///set width of component on screen
		virtual void setWidth(const int& w);
		///set all positions and dimensions of component at once
		void setDimensionsAll(const int& xpos, const int& ypos, const int& w, const int& h);

		///return screen x-position of component
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to get the real screen position, use getRealXPos(), to find in CComponentsItem sub classes
		int getXPos();
		///return screen y-position of component
		///Note: position of bound components (items) means position related within parent form, not for screen!
		///to get the real screen position, use getRealYPos(), to find in CComponentsItem sub classes
		int getYPos();
		///return height of component
		int getHeight();
		///return width of component
		int getWidth();

		///return/set (pass through) width and height of component
		void getSize(int* w, int* h){*w=width; *h=height;}
		///return/set (pass through) position and dimensions of component at once
		void getDimensions(int* xpos, int* ypos, int* w, int* h){*xpos=x; *ypos=y; *w=width; *h=height;}

		///set frame thickness
		void setFrameThickness(const int& thickness);
		///return of frame thickness
		int getFrameThickness() const {return fr_thickness;}
		///set frame color
		void setColorFrame(fb_pixel_t color){col_frame = color;}

		void set2ndColor(fb_pixel_t col_2nd){cc_body_gradient_2nd_col = col_2nd;}

		///get frame color
		fb_pixel_t getColorFrame(){return col_frame;}
		///get body color
		fb_pixel_t getColorBody(){return col_body;}
		///get shadow color
		fb_pixel_t getColorShadow(){return col_shadow;}

		///set body color
		void setColorBody(fb_pixel_t color){col_body = color;}
		///set shadow color
		void setColorShadow(fb_pixel_t color){col_shadow = color;}
		///set all basic framebuffer element colors at once
		///Note: Possible color values are defined in "gui/color.h" and "gui/color_custom.h"
		void setColorAll(fb_pixel_t color_frame, fb_pixel_t color_body, fb_pixel_t color_shadow){col_frame = color_frame; col_body = color_body; col_shadow = color_shadow;};

		///set corner types
		///Possible corner types are defined in CFrameBuffer (see: driver/framebuffer.h)
		///Note: default values are given from settings and corner radius sizes are predefined in /system/settings.h
		virtual void setCornerType(const int& type);
		///set corner radius and type
		virtual void setCorner(const int& radius, const int& type = CORNER_ALL);
		///get corner types
		int getCornerType(){return corner_type;};
		///get corner radius
		int getCornerRadius(){return corner_rad;};

		///switch shadow on/off
		void setShadowWidth(const int& shadow_width){if (shadow_w != shadow_width) shadow_w = shadow_width;}
		/**1st parameter requires defines CC_SHADOW_ON (default), CC_SHADOW_OFF, CC_SHADOW_BOTTOM or CC_SHADOW_RIGHT, see also cc_types.h
		 * 2nd parameter defines shadow width, default = defined by system
		 * 3rd parameter forces paint of shadow layer, default = false, Note: default shadow will paint only on first paint, use 3rd parameter=true ignores this
		*/
		void enableShadow(int mode = CC_SHADOW_ON, const int& shadow_width = -1, bool force_paint = false);
		///switch shadow off
		void disableShadow(){enableShadow(CC_SHADOW_OFF);}
		///return current schadow width
		int getShadowWidth(){return shadow_w;}

		///paint caching for body and shadow, see also cc_paint_cache NOTE: has no effect if paint_bg = false
		void enablePaintCache(bool enable = true);
		///disable paint caching for body and shadow
		void disablePaintCache(){enablePaintCache(false);}

		///returns paint mode, true=item was painted
		bool isPainted();
		///allows paint of elementary item parts (shadow, frame and body), similar as background, set it usually to false, if item used in a form, returns true, if mode has changed, also cleans screnn buffer
		bool doPaintBg(bool do_paint);
		///allows paint frame around body, default true , NOTE: ignored if frame width = 0
		void enableFrame(bool enable = true, const int& frame_width = -1){cc_enable_frame = enable; setFrameThickness(frame_width == -1 ? fr_thickness : frame_width);}
		///disallow paint frame around body
		void disableFrame(){enableFrame(false);}
		///enable/disable background buffering, default action = enable, see also cc_save_bg
		void enableSaveBg(bool save_bg = true);
		///disable background buffering, does the same like enableSaveBg(false), NOTE: cleans existant pixbuffer content!
		void disableSaveBg(){enableSaveBg(false);}
		///returns background buffering mode. Mode is assigned with paint() or enableSaveBg()/disableSaveBg())
		bool SaveBg(){return cc_save_bg;}

		///allow/disalows paint of item and its contents, but initialize of other properties are not touched
		///this can be understood as a counterpart to isPainted(), but before paint and value of is_painted is modified temporarily till next paint of item //TODO: is this sufficiently?
		void allowPaint(bool allow);
		///returns visibility mode
		bool paintAllowed();
		/**Overrides internal firstpaint and is_painted modes to provoke full repaint of item.
		 * This should help to paint items if they already painted and have existing instances and repaint is required but the internal
		 * performance protection would prevent this.
		 *
		 * @Note	add this method before paint() is called, because firstpaint and is_painted modes
		 * 			will be reset after callt paint() method
		 * @see		allowPaint(), isPainted(), firstpaint, is_painted
		*/
		void forceRePaint(){firstPaint = true; is_painted = false;};

		///set color gradient on/off, returns true if gradient mode was changed
		bool enableColBodyGradient(const int& enable_mode, const fb_pixel_t& sec_color = 255 /*=COL_BACKGROUND*/, const int& direction = 1 /*CFrameBuffer::gradientVertical*/);
		///disable color gradient, returns true if gradient mode was changed
		bool disableColBodyGradient(){return enableColBodyGradient(CC_COLGRAD_OFF);}
		///set color gradient properties, possible parameter values for mode and intensity to find in CColorGradient, in driver/framebuffer.h>
		void setColBodyGradient(const int& mode, const int& direction  = 1 /*CFrameBuffer::gradientVertical*/, const fb_pixel_t& sec_color = 255 /*=COL_BACKGROUND*/, const int& intensity = CColorGradient::normal, uint8_t v_min=0x40, uint8_t v_max=0xE0, uint8_t s=0xC0)
						{	cc_body_gradient_intensity=intensity;
							cc_body_gradient_intensity_v_min=v_min;
							cc_body_gradient_intensity_v_max=v_max;
							cc_body_gradient_saturation=s;
							enableColBodyGradient(mode, sec_color, direction);}
		///gets current color gradient mode
		int getColBodyGradientMode(){return cc_body_gradient_enable;}

		///abstract: paint item, arg: do_save_bg see paintInit() above
		virtual void paint(const bool &do_save_bg = CC_SAVE_SCREEN_YES) = 0;
		///paint item, same like paint(CC_SAVE_SCREEN_NO) but without any argument
		void paint0(){paint(CC_SAVE_SCREEN_NO);}
		///paint item, same like paint(CC_SAVE_SCREEN_YES) but without any argument
		void paint1(){paint(CC_SAVE_SCREEN_YES);}

		/**paint item with blink effect
		 * This should work with all cc item types.
		 *
		 * @return bool			returns true if effect is successful started
		 *
		 * @param[in] CComponentsTimer*	pointer to timer object, Note: This object must be created and distroy outside
		 * 				of this methode.
		 * @see				overloaded version of paintBlink()
		*/
		bool paintBlink(CComponentsTimer* Timer);

		/**paint item with blink effect
		 * This should work with all cc item types.
		 *
		 * @return bool			returns true if effect is successful started
		 *
		 * @param[in] interval		optional, interval time as int, default =  1
		 * @param[in] is_nano		optional, time mode as bool, default = false means as seconds, true means nano seconds.
		 *
		 * @see				take a look into test menu class for examples.
		 * 				cancelBlink()
		 *
		 * 				NOTE: If you want to use enbedded items from a cc form (e.g. with gettCCItem(ID))
		 * 				you must cast into current item type. e.g.:
		 * 					CComponentsItemBla* item = static_cast<CComponentsItemBla*>(form->getCCItem(2));
		 * 				and it's possible you must remove from screen before e.g.:
		 * 					item->kill();
		*/
		bool paintBlink(const int& interval = 1, bool is_nano = false);

		/**Cancel blink effect
		 *
		 * @return bool			returns true if effect was successful canceled
		 *
		 * @param[in] keep_on_screen	optional, expects bool, default = false. means: item is not repainted after canceled effect
		 *
		 * @see				take a look into test menu class for examples
		 * 				NOTE: Effect must be started with paintBlink()
		*/
		bool cancelBlink(bool keep_on_screen = false);

		///signal on before paint fb layers, called before paint fb layers inside paintFbItems()
		sigc::signal<void> OnBeforePaintLayers;
		///signal on after paint fb layers, called after paint fb layers inside paintFbItems()
		sigc::signal<void> OnAfterPaintLayers;
		///signal on after paint background, called after paint of background box inside paintFbItems()
		sigc::signal<void> OnAfterPaintBg;

		/*!
		 Removes current item from screen and
		 restore last displayed background before item was painted and
		 ensures demage of already existing screen buffers too.
		*/
		virtual void hide();

		/**Erase or paint over rendered objects without restore of background, it's similar to paintBackgroundBoxRel() known
		 * from CFrameBuffer but with possiblity to define color, default color is COL_BACKGROUND_PLUS_0 (empty background)
		 *
		 * @return void
		 *
		 * @param[in] bg_color		optional, color, default color is current screen
		 * @param[in] corner_radius	optional, defined corner radius, default radius is the current defined radius
		 * @param[in] fblayer_type	optional, defines layer that to remove, default all layers (cc_fbdata_t) will remove
		 * 				possible layer types are:
		 * 				@li CC_FBDATA_TYPE_BGSCREEN,
		 * 				@li CC_FBDATA_TYPE_BOX,
		 * 				@li CC_FBDATA_TYPE_SHADOW_BOX,
		 * 				@li CC_FBDATA_TYPE_FRAME,
		 * 				@li CC_FBDATA_TYPE_BACKGROUND,
		 * @see
		 * 	cc_types.h
		 * 	gui/color.h
		 * 	driver/framebuffer.h
		 * @todo
		 *	Shadow paint must be reworked, because dimensions of shadow containes not the real defined size. Parts of item are killed too.
		 *
		*/
		void kill(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, const int& corner_radius = -1, const int& fblayer_type = ~CC_FBDATA_TYPES);

		/**Erase shadow around rendered item.
		 * This is similar with the kill() member, but shadow will be handled only.
		 *
		 * @return void
		 *
		 * @param[in] bg_color		optional, color, default color is current screen
		 * @param[in] corner_radius	optional, defined corner radius, default radius is the current defined radius
		 *
		 * @see
		 * 	kill()
		*/
		void killShadow(const fb_pixel_t& bg_color = COL_BACKGROUND_PLUS_0, const int& corner_radius = -1);

		void enableGradientBgCleanUp(bool enable = true) { cc_gradient_bg_cleanup = enable; }
		void disableGradientBgCleanUp(){ enableGradientBgCleanUp(false); }

		/**Sets an image path for body background, returns true if new image was applied.
		 *
		 * @return bool
		 *
		 * @param[in] image_path	Path to image.
		 *
		 * @see
		 * 	cc_body_image
		 * 	setBodyBGImageName()
		*/
		bool setBodyBGImage(const std::string& image_path);

		/**Sets an image name for body background, returns true if new image was applied.
		 *
		 * @return bool
		 *
		 * @param[in] image_name	Basename of image.
		 *
		 * @see
		 * 	cc_body_image
		 * 	setBodyBGImage()
		*/
		bool setBodyBGImageName(const std::string& image_name);
};

#endif

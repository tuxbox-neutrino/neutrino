/*
 * lua components text
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
 * (C) 2014 Sven Hoefer (svenhoefer)
 * (C) 2015 Jacek Jendrzej (SatBaby)
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <global.h>

#include <system/debug.h>
#include <system/helpers.h>
#include <gui/components/cc.h>
#include <neutrino.h>

#include "luainstance.h"
#include "lua_cc_window.h"
#include "lua_cc_text.h"

CLuaInstCCText* CLuaInstCCText::getInstance()
{
	static CLuaInstCCText* LuaInstCCText = NULL;

	if(!LuaInstCCText)
		LuaInstCCText = new CLuaInstCCText();
	return LuaInstCCText;
}

CLuaCCText *CLuaInstCCText::CCTextCheck(lua_State *L, int n)
{
	return *(CLuaCCText **) luaL_checkudata(L, n, "ctext");
}

void CLuaInstCCText::CCTextRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",              CLuaInstCCText::CCTextNew },
		{ "paint",            CLuaInstCCText::CCTextPaint },
		{ "hide",             CLuaInstCCText::CCTextHide },
		{ "setText",          CLuaInstCCText::CCTextSetText },
		{ "getLines",         CLuaInstCCText::CCTextGetLines },
		{ "scroll",           CLuaInstCCText::CCTextScroll },
		{ "setCenterPos",     CLuaInstCCText::CCTextSetCenterPos },
		{ "enableUTF8",       CLuaInstCCText::CCTextEnableUTF8 },
		{ "setDimensionsAll", CLuaInstCCText::CCTextSetDimensionsAll },
		{ "__gc",             CLuaInstCCText::CCTextDelete },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, "ctext");
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, "ctext");
}

int CLuaInstCCText::CCTextNew(lua_State *L)
{
	lua_assert(lua_istable(L,1));

	CLuaCCWindow* parent = NULL;
	lua_Integer x=10, y=10, dx=-1, dy=-1;
	std::string text = "";
	std::string tmpMode       = "";
	lua_Integer mode          = CTextBox::AUTO_WIDTH;
	lua_Integer font_text     = SNeutrinoSettings::FONT_TYPE_MENU;
	lua_Unsigned color_text   = (lua_Unsigned)COL_MENUCONTENT_TEXT;
	lua_Unsigned color_frame  = (lua_Unsigned)COL_FRAME_PLUS_0;
	lua_Unsigned color_body   = (lua_Unsigned)COL_MENUCONTENT_PLUS_0;
	lua_Unsigned color_shadow = (lua_Unsigned)COL_SHADOW_PLUS_0;

	tableLookup(L, "parent",    (void**)&parent);
	tableLookup(L, "x",         x);
	tableLookup(L, "y",         y);
	tableLookup(L, "dx",        dx);
	tableLookup(L, "dy",        dy);
	tableLookup(L, "text",      text);
	//to prevent possible ambiguity of text mode, use 'text_mode' instead 'mode' in api
	tableLookup(L, "text_mode", tmpMode);
	tableLookup(L, "mode",      tmpMode); //NOTE: parameter 'mode' deprecated, but still available for downward compatibility
	tableLookup(L, "font_text", font_text);
	if (font_text >= SNeutrinoSettings::FONT_TYPE_COUNT || font_text < 0)
		font_text = SNeutrinoSettings::FONT_TYPE_MENU;

	lua_Integer shadow_mode = CC_SHADOW_OFF;
	if (!tableLookup(L, "has_shadow", shadow_mode)) {
		std::string tmp1 = "false";
		if (tableLookup(L, "has_shadow", tmp1))
			paramBoolDeprecated(L, tmp1.c_str());
		shadow_mode = (tmp1 == "true" || tmp1 == "1" || tmp1 == "yes");
	}

	tableLookup(L, "color_text",   color_text);
	tableLookup(L, "color_frame",  color_frame);
	tableLookup(L, "color_body",   color_body);
	tableLookup(L, "color_shadow", color_shadow);

	color_text   = checkMagicMask(color_text);
	color_frame  = checkMagicMask(color_frame);
	color_body   = checkMagicMask(color_body);
	color_shadow = checkMagicMask(color_shadow);

	if (!tmpMode.empty()) {
		table_key txt_align[] = {
			{ "ALIGN_AUTO_WIDTH",		CTextBox::AUTO_WIDTH },
			{ "ALIGN_AUTO_HIGH",		CTextBox::AUTO_HIGH },
			{ "ALIGN_SCROLL",		CTextBox::SCROLL },
			{ "ALIGN_CENTER",		CTextBox::CENTER },
			{ "ALIGN_RIGHT",		CTextBox::RIGHT },
			{ "ALIGN_TOP",			CTextBox::TOP },
			{ "ALIGN_BOTTOM",		CTextBox::BOTTOM },
			{ "ALIGN_NO_AUTO_LINEBREAK",	CTextBox::NO_AUTO_LINEBREAK },
			{ "DECODE_HTML",		0 },
			{ NULL,				0 }
		};
		mode = 0;
		for (int i = 0; txt_align[i].name; i++) {
			if (tmpMode.find(txt_align[i].name) != std::string::npos)
				mode |= txt_align[i].code;
		}
		if (tmpMode.find("DECODE_HTML") != std::string::npos)
			htmlEntityDecode(text);
	}

	CComponentsForm* pw = (parent && parent->w) ? parent->w->getBodyObject() : NULL;
	if(pw){
		if(dy < 1)
			dy = pw->getHeight();
		if(dx < 1)
			dx = pw->getWidth();
	}
	if(dx < 1)
		dx = 100;
	if(dy < 1)
		dy = 100;

	CLuaCCText **udata = (CLuaCCText **) lua_newuserdata(L, sizeof(CLuaCCText *));
	*udata = new CLuaCCText();
	(*udata)->ct = new CComponentsText(x, y, dx, dy, text, mode, g_Font[font_text], 0 /*TODO: style not passed*/, pw, shadow_mode, (fb_pixel_t)color_text, (fb_pixel_t)color_frame, (fb_pixel_t)color_body, (fb_pixel_t)color_shadow);
	(*udata)->parent = pw;
	(*udata)->mode = mode;
	(*udata)->font_text = font_text;
	luaL_getmetatable(L, "ctext");
	lua_setmetatable(L, -2);
	return 1;
}

int CLuaInstCCText::CCTextPaint(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	bool do_save_bg = true;
	if (!tableLookup(L, "do_save_bg", do_save_bg)) {
		std::string tmp = "true";
		if (tableLookup(L, "do_save_bg", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		do_save_bg = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->ct->paint(do_save_bg);
	return 0;
}

int CLuaInstCCText::CCTextHide(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	bool tmp1 = false;
	std::string tmp2 = "false";
	if ((tableLookup(L, "no_restore", tmp1)) || (tableLookup(L, "no_restore", tmp2)))
		obsoleteHideParameter(L);

	if (D->parent) {
		D->ct->setText("", D->mode, g_Font[D->font_text]);
		D->ct->paint();
	} else
		D->ct->hide();
	return 0;
}

int CLuaInstCCText::CCTextSetText(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	std::string text = "";
	lua_Integer mode = D->mode;
	lua_Integer font_text = D->font_text;
	if (lua_istable(L, -1)){
		tableLookup(L, "text", text);
		tableLookup(L, "mode", mode);
		tableLookup(L, "font_text", font_text);
	}
	D->ct->setText(text, mode, g_Font[font_text]);
	return 0;
}

int CLuaInstCCText::CCTextGetLines(lua_State *L)
{
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	lua_Integer lines = 0;
	if (lua_gettop(L) == 2) {
		const char* Text = luaL_checkstring(L, 2);
		lines = (lua_Integer)CTextBox::getLines(Text);
	}
	else {
		CTextBox* ctb = D->ct->getCTextBoxObject();
		D->ct->initCCText();
		if (ctb)
			lines = (lua_Integer)ctb->getLines();
	}

	lua_pushinteger(L, lines);
	return 1;
}

int CLuaInstCCText::CCTextScroll(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	std::string tmp = "true";
	tableLookup(L, "dir", tmp);
	bool scrollDown = (tmp == "down" || tmp == "1");
	lua_Integer pages = 1;
	tableLookup(L, "pages", pages);

	//get the textbox instance from lua object and use CTexBbox scroll methods
	CTextBox* ctb = D->ct->getCTextBoxObject();
	if (ctb) {
		if (pages == -1)
			pages = ctb->getPages();
		ctb->enableBackgroundPaint(true);
		if (scrollDown)
			ctb->scrollPageDown(pages);
		else
			ctb->scrollPageUp(pages);
		ctb->enableBackgroundPaint(false);
	}
	return 0;
}

int CLuaInstCCText::CCTextSetCenterPos(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;
	lua_Integer  tmp_along_mode, along_mode = CC_ALONG_X | CC_ALONG_Y;
	tableLookup(L, "along_mode", tmp_along_mode);

	if (tmp_along_mode & CC_ALONG_X || tmp_along_mode & CC_ALONG_Y)
		along_mode=tmp_along_mode;

	D->ct->setCenterPos(along_mode);
	return 0;
}

int CLuaInstCCText::CCTextEnableUTF8(lua_State *L)
{
	lua_assert(lua_istable(L,1));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;

	bool utf8_encoded = true;
	if (!tableLookup(L, "utf8_encoded", utf8_encoded)) {
		std::string tmp = "true";
		if (tableLookup(L, "utf8_encoded", tmp))
			paramBoolDeprecated(L, tmp.c_str());
		utf8_encoded = (tmp == "true" || tmp == "1" || tmp == "yes");
	}
	D->ct->enableUTF8(utf8_encoded);
	return 0;
}

int CLuaInstCCText::CCTextSetDimensionsAll(lua_State *L)
{
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;
	lua_Integer x = luaL_checkint(L, 2);
	lua_Integer y = luaL_checkint(L, 3);
	lua_Integer w = luaL_checkint(L, 4);
	lua_Integer h = luaL_checkint(L, 5);
	if(x>-1 && y > -1 && w > 1 && h > 1){
		if (h > (lua_Integer)CFrameBuffer::getInstance()->getScreenHeight())
			h = (lua_Integer)CFrameBuffer::getInstance()->getScreenHeight();
		if (w > (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth())
			w = (lua_Integer)CFrameBuffer::getInstance()->getScreenWidth();
		if(x > w)
			x = 0;
		if(y > h)
			y = 0;
		D->ct->setDimensionsAll(x,y,w,h);
	}
	return 0;
}

int CLuaInstCCText::CCTextDelete(lua_State *L)
{
	LUA_DEBUG("CLuaInstCCText::%s %d\n", __func__, lua_gettop(L));
	CLuaCCText *D = CCTextCheck(L, 1);
	if (!D) return 0;
	delete D;
	return 0;
}

/*
 * neutrino-mp lua to c++ bridge
 *
 * (C) 2013 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>
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
#ifndef _LUAINSTANCE_H
#define _LUAINSTANCE_H

/* LUA Is C */
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <driver/fb_window.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/messagebox.h>
#include <gui/components/cc.h>
#include <vector>

#include "luainstance_helpers.h"

#define LUA_API_VERSION_MAJOR 1
#define LUA_API_VERSION_MINOR 22


class CLuaHintbox
{
	public:
		CHintBox *b;
		char *caption;
		CLuaHintbox();
		~CLuaHintbox();
};

class CLuaMessagebox
{
	public:
		CMessageBox *b;
		CLuaMessagebox();
		~CLuaMessagebox();
};

class CLuaCWindow
{
	public:
		CComponentsWindow *w;
		CLuaCWindow() { w = NULL; }
		~CLuaCWindow() { delete w; }
};

class CLuaSignalBox
{
	public:
		CSignalBox *s;
		CComponentsForm *parent;
		CLuaSignalBox() { s = NULL; parent = NULL;}
		~CLuaSignalBox() { if (parent == NULL) delete s; }
};

class CLuaComponentsText
{
	public:
		CComponentsText *ct;
		CComponentsForm *parent;
		int mode, font_text;
		CLuaComponentsText() { ct = NULL; parent = NULL; mode = 0; font_text = 0;}
		~CLuaComponentsText() { if (parent == NULL) delete ct; }
};

class CLuaPicture
{
	public:
		CComponentsPicture *cp;
		CComponentsForm *parent;
		CLuaPicture() { cp = NULL; parent = NULL; }
		~CLuaPicture() { if (parent == NULL) delete cp; }
};

/* inspired by Steve Kemp http://www.steve.org.uk/ */
class CLuaInstance
{
	static const char className[];
	static const luaL_Reg methods[];
	static const luaL_Reg menu_methods[];
	static CLuaData *CheckData(lua_State *L, int narg);
public:
	CLuaInstance();
	~CLuaInstance();
	void runScript(const char *fileName, std::vector<std::string> *argv = NULL, std::string *result_code = NULL, std::string *result_string = NULL, std::string *error_string = NULL);
	void abortScript();

	enum {
		DYNFONT_NO_ERROR      = 0,
		DYNFONT_MAXIMUM_FONTS = 1,
		DYNFONT_TO_WIDE       = 2,
		DYNFONT_TOO_HIGH      = 3
	};

	// Example: runScript(fileName, "Arg1", "Arg2", "Arg3", ..., NULL);
	//	Type of all parameters: const char*
	//	The last parameter to NULL is imperative.
	void runScript(const char *fileName, const char *arg0, ...);

private:
	lua_State* lua;
	void registerFunctions();

	static int GetRevision(lua_State *L);
	static int NewWindow(lua_State *L);
	static int saveScreen(lua_State *L);
	static int restoreScreen(lua_State *L);
	static int deleteSavedScreen(lua_State *L);
	static int PaintBox(lua_State *L);
	static int paintHLineRel(lua_State *L);
	static int paintVLineRel(lua_State *L);
	static int PaintIcon(lua_State *L);
	static int RenderString(lua_State *L);
	static int getRenderWidth(lua_State *L);
	static int FontHeight(lua_State *L);
	static int GetInput(lua_State *L);
	static int GCWindow(lua_State *L);
	static int Blit(lua_State *L);
	static int GetLanguage(lua_State *L);
	static int runScriptExt(lua_State *L);
	static int GetSize(lua_State *L);
	static int DisplayImage(lua_State *L);

	static int strFind(lua_State *L);
	static int strSub(lua_State *L);

	void HintboxRegister(lua_State *L);
	static int HintboxNew(lua_State *L);
	static int HintboxDelete(lua_State *L);
	static int HintboxExec(lua_State *L);
	static int HintboxPaint(lua_State *L);
	static int HintboxHide(lua_State *L);
	static CLuaHintbox *HintboxCheck(lua_State *L, int n);

	void MessageboxRegister(lua_State *L);
	static int MessageboxExec(lua_State *L);
	static CLuaMessagebox *MessageboxCheck(lua_State *L, int n);

	void CWindowRegister(lua_State *L);
	static int CWindowNew(lua_State *L);
	static CLuaCWindow *CWindowCheck(lua_State *L, int n);
	static int CWindowPaint(lua_State *L);
	static int CWindowHide(lua_State *L);
	static int CWindowSetCaption(lua_State *L);
	static int CWindowSetWindowColor(lua_State *L);
	static int CWindowPaintHeader(lua_State *L);
	static int CWindowGetHeaderHeight(lua_State *L);
	static int CWindowGetFooterHeight(lua_State *L);
	static int CWindowGetHeaderHeight_dep(lua_State *L); // function 'header_height' is deprecated
	static int CWindowGetFooterHeight_dep(lua_State *L); // function 'footer_height' is deprecated
	static int CWindowSetCenterPos(lua_State *L);
	static int CWindowDelete(lua_State *L);

	static CLuaSignalBox *SignalBoxCheck(lua_State *L, int n);
	static void SignalBoxRegister(lua_State *L);
	static int SignalBoxNew(lua_State *L);
	static int SignalBoxPaint(lua_State *L);
	static int SignalBoxSetCenterPos(lua_State *L);
	static int SignalBoxDelete(lua_State *L);

	static CLuaComponentsText *ComponentsTextCheck(lua_State *L, int n);
	static void ComponentsTextRegister(lua_State *L);
	static int ComponentsTextNew(lua_State *L);
	static int ComponentsTextPaint(lua_State *L);
	static int ComponentsTextHide(lua_State *L);
	static int ComponentsTextSetText(lua_State *L);
	static int ComponentsTextScroll(lua_State *L);
	static int ComponentsTextSetCenterPos(lua_State *L);
	static int ComponentsTextEnableUTF8(lua_State *L);
	static int ComponentsTextDelete(lua_State *L);

	static CLuaPicture *CPictureCheck(lua_State *L, int n);
	static void CPictureRegister(lua_State *L);
	static int CPictureNew(lua_State *L);
	static int CPicturePaint(lua_State *L);
	static int CPictureHide(lua_State *L);
	static int CPictureSetPicture(lua_State *L);
	static int CPictureSetCenterPos(lua_State *L);
	static int CPictureDelete(lua_State *L);

	static int checkVersion(lua_State *L);
	static int createChannelIDfromUrl(lua_State *L);
	static int enableInfoClock(lua_State *L);
	static int getDynFont(lua_State *L);

};

#endif /* _LUAINSTANCE_H */

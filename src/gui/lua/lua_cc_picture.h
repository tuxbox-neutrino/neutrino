/*
 * lua components picture
 *
 * (C) 2014-2015 M. Liebmann (micha-bbg)
 * (C) 2014 Thilo Graf (dbt)
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

#ifndef _LUACCPICTURE_H
#define _LUACCPICTURE_H

class CLuaCCPicture
{
	public:
		CComponentsPicture *cp;
		CComponentsForm *parent;
		CLuaCCPicture() { cp = NULL; parent = NULL; }
		~CLuaCCPicture() { if (parent == NULL) delete cp; }
};

class CLuaInstCCPicture
{
	public:
		CLuaInstCCPicture() {};
		~CLuaInstCCPicture() {};
		static CLuaInstCCPicture* getInstance();
		static void CCPictureRegister(lua_State *L);

	private:
		static CLuaCCPicture *CCPictureCheck(lua_State *L, int n);
		static int CCPictureNew(lua_State *L);
		static int CCPicturePaint(lua_State *L);
		static int CCPictureHide(lua_State *L);
		static int CCPictureSetPicture(lua_State *L);
		static int CCPictureSetCenterPos(lua_State *L);
		static int CCPictureDelete(lua_State *L);
};

#endif //_LUACCPICTURE_H

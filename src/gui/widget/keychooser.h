/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'

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

#ifndef __keychooser__
#define __keychooser__

#include <string>

#include <driver/neutrino_msg_t.h>

#include "menue.h"

class CFrameBuffer;

class CKeyChooser : public CMenuWidget
{
	private:
		unsigned int *		key;
		std::string		keyName;

	public:
		CKeyChooser(unsigned int * const Key, const neutrino_locale_t title, const std::string & Icon = "");
		~CKeyChooser();
		void reinitName();
		void paint();
		const std::string & getKeyName(){return keyName;};
};

class CKeyChooserItem : public CMenuTarget
{
	private:
		unsigned int *key;
		neutrino_locale_t name;

	public:
		CKeyChooserItem(unsigned int *Key, const neutrino_locale_t Name)
		{
			key = Key;
			name = Name;
		}

		int exec(CMenuTarget* parent, const std::string & /*actionKey*/);
};

class CKeyRemoverItem : public CMenuTarget
{
	private:
		unsigned int *key;

	public:
		CKeyRemoverItem(unsigned int *Key)
		{
			key = Key;
		};

		int exec(CMenuTarget* parent, const std::string & /*actionKey*/);
};

#endif

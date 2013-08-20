/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the 
	Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
	Boston, MA  02110-1301, USA.
*/


#ifndef __update__
#define __update__

#include <gui/widget/menue.h>
#include <gui/widget/progressstatus.h>
#include <gui/widget/progresswindow.h>

#include <driver/framebuffer.h>

#include <string>

class CFlashUpdate : public CProgressWindow
{
 private:
	std::string filename;
	std::string file_md5;
	std::string sysfs;
	char	fileType;
	int 	width;
	
	std::string installedVersion;
	std::string newVersion;
	int	menu_ret;
	int softupdate_mode;
	
	bool selectHttpImage(void);
	bool getUpdateImage(const std::string & version);
	bool checkVersion4Update();
	
 public:
	CFlashUpdate();
	int exec( CMenuTarget* parent, const std::string & actionKey );

};

class CFlashExpert : public CProgressWindow
{
	private:
		int selectedMTD;
		int width;

		void showMTDSelector(const std::string & actionkey);
		void showFileSelector(const std::string & actionkey);

		bool checkSize(int mtd, std::string &backupFile);
		void readmtd(int readmtd);

	public:
		CFlashExpert();
		static CFlashExpert* getInstance();
		int exec(CMenuTarget* parent, const std::string & actionKey);
		void writemtd(const std::string & filename, int mtdNumber);

};


#endif

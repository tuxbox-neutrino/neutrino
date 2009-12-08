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
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/widget/messagebox.h>
#include <gui/widget/helpbox.h>
#include <gui/widget/icons.h>

#include <global.h>
#include <neutrino.h>

Helpbox::Helpbox()
{
}

Helpbox::~Helpbox()
{
  	for (ContentLines::iterator it = m_lines.begin();
  		 it != m_lines.end(); it++)
 	{
 		for (std::vector<Drawable*>::iterator it2 = it->begin();
 			 it2 != it->end(); it2++)
 		{
 			delete *it2;
 		}
  	}
}

void Helpbox::show(const neutrino_locale_t Caption, const int Width, int timeout)
{

 	CMessageBox msgBox(Caption, m_lines, Width, NEUTRINO_ICON_INFO, CMessageBox::mbrBack,CMessageBox::mbBack);
	msgBox.exec(timeout);
}

void Helpbox::addLine(const char *text)
{
	std::vector<Drawable*> v;
	Drawable *d = new DText(text);
	v.push_back(d);
	m_lines.push_back(v);
}

void Helpbox::addLine(std::string& text)
{
	std::vector<Drawable*> v;
	Drawable *d = new DText(text);
	v.push_back(d);
	m_lines.push_back(v);
}

void Helpbox::addLine(const char *icon, const char *text)
{
	std::vector<Drawable*> v;
	Drawable *di = new DIcon(icon);
	Drawable *dt = new DText(text);
	v.push_back(di);
	v.push_back(dt);
	m_lines.push_back(v);
}

void Helpbox::addLine(std::string& icon, std::string& text)
{
	std::vector<Drawable*> v;
	Drawable *di = new DIcon(icon);
	Drawable *dt = new DText(text);
	v.push_back(di);
	v.push_back(dt);
	m_lines.push_back(v);
}

void Helpbox::addPagebreak(void)
{
	std::vector<Drawable*> v;
	Drawable *p = new DPagebreak();
	v.push_back(p);
	m_lines.push_back(v);
}


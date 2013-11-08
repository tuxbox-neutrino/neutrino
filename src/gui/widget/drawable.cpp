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

#include <iostream>

#include <gui/widget/drawable.h>

#include <global.h>
#include <neutrino.h>

Drawable::Drawable()
{
}

Drawable::~Drawable()
{
}

int Drawable::getWidth(void)
{
	return m_width;
}

int Drawable::getHeight(void)
{
	return m_height;
}

Drawable::DType Drawable::getType(void)
{
	return Drawable::DTYPE_DRAWABLE;
}


// ------------------------------------------------------------------------------
DText::DText(std::string& text)
{
	m_text = text;
	init();
}

DText::DText(const char *text)
{
	m_text = std::string(text);
	init();
}

void DText::init()
{
	m_width = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth(m_text, true); // UTF-8
	m_height = g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getHeight();
}


void DText::draw(CFBWindow *window, int x, int y, int width)
{
	window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_MENU], x, y + m_height, width,
						 m_text.c_str(), COL_MENUCONTENT_TEXT, 0, true); // UTF-8
}

void DText::print(void)
{
	std::cout << " text: " << m_text;
}

// ------------------------------------------------------------------------------
DIcon::DIcon(std::string& icon)
{
	m_icon = icon;
	init();
}

DIcon::DIcon(const char *icon)
{
	m_icon = std::string(icon);
	init();
}

void DIcon::init()
{
	m_height = 16;
	m_width = 16;
}

void DIcon::draw(CFBWindow *window, int x, int y, int /*width*/)
{
	window->paintIcon(m_icon.c_str(), x, y);
}

void DIcon::print(void)
{
	std::cout << " icon: " << m_icon;
}

// ------------------------------------------------------------------------------
DPagebreak::DPagebreak()
{
	m_height = 0;
	m_width = 0;
}

void DPagebreak::draw(CFBWindow */*window*/, int /*x*/, int /*y*/, int /*width*/)
{
// 	window->RenderString(g_Font[SNeutrinoSettings::FONT_TYPE_MENU],
// 						 x, y + m_height, width, "<pagebreak>",
// 						 COL_MENUCONTENT_TEXT, 0, true); // UTF-8
}

void DPagebreak::print(void)
{
	std::cout << "<pagebreak>";
}

Drawable::DType DPagebreak::getType(void)
{
	return Drawable::DTYPE_PAGEBREAK;
}

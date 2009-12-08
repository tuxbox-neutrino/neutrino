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


#ifndef __drawable__
#define __drawable__

#include <driver/fb_window.h>
#include <system/localize.h>

#include <string>
#include <vector>

class Drawable;

typedef std::vector<std::vector<Drawable*> > ContentLines;


/**
 * The base class for items which can be drawn on a CFBWindow.
 */
class Drawable
{
public:

	enum DType {
		DTYPE_DRAWABLE,
		DTYPE_PAGEBREAK
	};

	virtual ~Drawable();
	
	/**
	 * Overwrite this method in subclasses to draw on the window
	 *
	 * @param window the window to draw on
	 * @param x x component of the top left corner
	 * @param y y component of the top left corner
	 */
	virtual void draw(CFBWindow *window, int x, int y, int width) = 0;

	virtual int getWidth(void);
	
	virtual int getHeight(void);

	/**
	 * Overwrite this method in subclasses to print some info
	 * about the content. Mainly used for debuging ;) 
	 */
	virtual void print(void) = 0;

	/**
	 * Returns the type of this drawable. Used to distinguish between
	 * drawing objects and control objects like pagebreaks.
	 * @return the type of this drawable.
	 */
	virtual DType getType();

protected:

	Drawable();

	int m_height;

	int m_width;

private:	
	
};

/**
 * This class draws a given string.
 */
class DText : public Drawable
{
public:
	DText(std::string& text);

	DText(const char *text);

	void init();

	void draw(CFBWindow *window, int x, int y, int width);
	
	void print();

protected:

	std::string m_text;

};


/**
 * This class draws a given icon.
 */
class DIcon : public Drawable
{
public:
	DIcon(std::string& icon);

	DIcon(const char  *icon);

	void init();

	void draw(CFBWindow *window, int x, int y, int width);
	
	void print();

protected:

	std::string m_icon;
};

/**
 * This class is used as a control object and forces a new page
 * in scrollable windows.
 */
class DPagebreak : public Drawable
{
public:
	DPagebreak();

	void draw(CFBWindow *window, int x, int y, int width);
	
	void print();

	DType getType();

protected:

private:
	
};

#endif
